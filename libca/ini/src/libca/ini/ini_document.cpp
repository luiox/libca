#include "libca/ini/ini_document.hpp"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <utility>

namespace ca::ini {

namespace {

// Utf8StringRef 转 std::string（用于错误消息拼接等临时用途）。
std::string to_std(const ca::str::Utf8StringRef& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

// 把 std::string 经 arena 入池得到 Utf8StringRef。
ca::str::Utf8StringRef intern_std(ca::str::Utf8StringArena& arena, const std::string& s) {
    return arena.intern(ca::str::Utf8String(
        reinterpret_cast<const ca::u8*>(s.data()), s.size()));
}

// 判断 value 末尾引号是否被反斜杠转义：从末尾引号前一个字节起向前数连续反斜杠，
// 奇数个表示转义（该引号不是闭合引号）。
bool trailing_quote_escaped(const ca::u8* data, ca::usize quote_pos) {
    ca::usize backslashes = 0;
    while (quote_pos > 0 && data[quote_pos - 1] == '\\') {
        ++backslashes;
        --quote_pos;
    }
    return (backslashes % 2) != 0;
}

// 把 ASCII 字符串转小写（仅用于 bool 判定）。
std::string ascii_lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

}  // namespace

// ============================================================================
// 构造 / 析构 / 移动（arena_ 声明先于 records_，保证析构逆序：先树后池）
// ============================================================================

IniDocument::IniDocument() : arena_(), records_(), public_lines_() {}

IniDocument::~IniDocument() = default;

IniDocument::IniDocument(IniDocument&& other) noexcept
    : arena_(std::move(other.arena_)),
      records_(std::move(other.records_)),
      public_lines_(std::move(other.public_lines_)),
      section_index_(std::move(other.section_index_)),
      key_index_(std::move(other.key_index_)),
      default_line_ending_(std::move(other.default_line_ending_)) {}

IniDocument& IniDocument::operator=(IniDocument&& other) noexcept {
    if (this != &other) {
        arena_ = std::move(other.arena_);
        records_ = std::move(other.records_);
        public_lines_ = std::move(other.public_lines_);
        section_index_ = std::move(other.section_index_);
        key_index_ = std::move(other.key_index_);
        default_line_ending_ = std::move(other.default_line_ending_);
    }
    return *this;
}

ca::str::Utf8StringArena& IniDocument::arena() noexcept { return arena_; }

// ============================================================================
// 内部查找
// ============================================================================

const ca::str::Utf8StringRef* IniDocument::find_value(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    auto section_it = key_index_.find(section);
    if (section_it == key_index_.end()) return nullptr;
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) return nullptr;
    return &records_[key_it->second].line.value;
}

// ============================================================================
// 查询
// ============================================================================

bool IniDocument::has_section(const ca::str::Utf8StringRef& section) const {
    if (section.is_empty()) {
        // 空字符串表示全局区：检查 key_index_ 是否有该 key
        return key_index_.find(section) != key_index_.end();
    }
    return section_index_.find(section) != section_index_.end();
}

bool IniDocument::has(const ca::str::Utf8StringRef& section,
                      const ca::str::Utf8StringRef& key) const {
    auto section_it = key_index_.find(section);
    if (section_it == key_index_.end()) return false;
    return section_it->second.find(key) != section_it->second.end();
}

ca::Result<ca::str::Utf8StringRef, ca::str::Utf8String> IniDocument::get(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    auto section_it = key_index_.find(section);
    if (section_it == key_index_.end()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("INI section not found: " + to_std(section)).c_str()));
    }
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("INI key not found: " + to_std(key)).c_str()));
    }
    return ca::Ok(records_[key_it->second].line.value);
}

ca::str::Utf8StringRef IniDocument::get_or(const ca::str::Utf8StringRef& section,
                                           const ca::str::Utf8StringRef& key,
                                           const ca::str::Utf8StringRef& default_value) const {
    const ca::str::Utf8StringRef* value = find_value(section, key);
    if (value != nullptr) {
        return *value;
    }
    return default_value;
}

// ============================================================================
// 类型化访问
// ============================================================================

namespace {

// 剥首尾配对的单/双引号。返回 std::string（用于数值/bool 解析）。
std::string strip_quotes_to_std(const ca::str::Utf8StringRef& value) {
    const ca::usize blen = value.byte_length();
    if (blen < 2) {
        return std::string(reinterpret_cast<const char*>(value.data()), blen);
    }
    const ca::u8 first = value.data()[0];
    const ca::u8 last = value.data()[blen - 1];
    if ((first == '"' || first == '\'') && first == last &&
        !trailing_quote_escaped(value.data(), blen - 1)) {
        return std::string(reinterpret_cast<const char*>(value.data() + 1), blen - 2);
    }
    return std::string(reinterpret_cast<const char*>(value.data()), blen);
}

}  // namespace

ca::Result<ca::i64, ca::str::Utf8String> IniDocument::get_int(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const ca::str::Utf8StringRef* value = find_value(section, key);
    if (value == nullptr) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("INI key not found: " + to_std(key)).c_str()));
    }
    const std::string s = strip_quotes_to_std(*value);
    try {
        std::size_t pos = 0;
        long long v = std::stoll(s, &pos);
        if (pos != s.size()) {
            return ca::Err(ca::str::Utf8String::from_cstr(
                ("INI value is not a valid integer: " + to_std(*value)).c_str()));
        }
        return ca::Ok(static_cast<ca::i64>(v));
    } catch (...) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("INI value is not a valid integer: " + to_std(*value)).c_str()));
    }
}

ca::Result<ca::f64, ca::str::Utf8String> IniDocument::get_double(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const ca::str::Utf8StringRef* value = find_value(section, key);
    if (value == nullptr) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("INI key not found: " + to_std(key)).c_str()));
    }
    const std::string s = strip_quotes_to_std(*value);
    try {
        std::size_t pos = 0;
        double v = std::stod(s, &pos);
        if (pos != s.size()) {
            return ca::Err(ca::str::Utf8String::from_cstr(
                ("INI value is not a valid number: " + to_std(*value)).c_str()));
        }
        return ca::Ok(static_cast<ca::f64>(v));
    } catch (...) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("INI value is not a valid number: " + to_std(*value)).c_str()));
    }
}

ca::Result<bool, ca::str::Utf8String> IniDocument::get_bool(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const ca::str::Utf8StringRef* value = find_value(section, key);
    if (value == nullptr) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("INI key not found: " + to_std(key)).c_str()));
    }
    const std::string s = ascii_lower(strip_quotes_to_std(*value));
    if (s == "true" || s == "yes" || s == "on" || s == "1") return ca::Ok(true);
    if (s == "false" || s == "no" || s == "off" || s == "0") return ca::Ok(false);
    return ca::Err(ca::str::Utf8String::from_cstr(
        ("INI value is not a valid boolean: " + to_std(*value)).c_str()));
}

// ============================================================================
// 编辑：set
// ============================================================================

void IniDocument::set(const ca::str::Utf8StringRef& section,
                      const ca::str::Utf8StringRef& key,
                      const ca::str::Utf8StringRef& value) {
    // 已存在的 key：改 value，复用保存的格式片段重建该行。
    auto section_it = key_index_.find(section);
    if (section_it != key_index_.end()) {
        auto key_it = section_it->second.find(key);
        if (key_it != section_it->second.end()) {
            auto& record = records_[key_it->second];
            record.line.value = arena_.intern(value);
            rebuild_key_raw(key_it->second);
            // 同步更新 public_lines_（Utf8StringRef 可拷贝，直接赋值）。
            IniLine updated;
            updated.kind = record.line.kind;
            updated.section = record.line.section;
            updated.key = record.line.key;
            updated.value = record.line.value;
            public_lines_[key_it->second] = updated;
            return;
        }
    }

    const auto newline = line_ending_for_new_line();
    const std::string sec = to_std(section);

    // 新 key 落在尚不存在的 section：先建 section 头。
    if (!sec.empty() && section_index_.find(section) == section_index_.end()) {
        if (!records_.empty() && !records_.back().raw.is_empty()) {
            detail::LineRecord blank;
            blank.line.kind = IniLineKind::Blank;
            blank.line_ending = intern_std(arena_, newline);
            records_.push_back(std::move(blank));
        }

        detail::LineRecord section_record;
        section_record.line.kind = IniLineKind::Section;
        section_record.line.section = arena_.intern(section);
        section_record.raw = intern_std(arena_, "[" + sec + "]");
        section_record.line_ending = intern_std(arena_, newline);
        records_.push_back(std::move(section_record));
        rebuild_index();
    }

    // 新增 key 行用统一的 "key = value" 格式。
    detail::LineRecord key_record;
    key_record.line.kind = IniLineKind::KeyValue;
    key_record.line.section = arena_.intern(section);
    key_record.line.key = arena_.intern(key);
    key_record.line.value = arena_.intern(value);
    key_record.key_suffix = intern_std(arena_, " ");
    key_record.separator = intern_std(arena_, "=");
    key_record.value_prefix = intern_std(arena_, " ");
    key_record.line_ending = intern_std(arena_, newline);
    key_record.value_quoted = false;
    key_record.raw = intern_std(arena_, to_std(key) + " = " + to_std(value));

    auto pos = find_insert_position(section);
    records_.insert(records_.begin() + static_cast<std::ptrdiff_t>(pos),
                    std::move(key_record));
    rebuild_index();
}

// ============================================================================
// 编辑：remove
// ============================================================================

bool IniDocument::remove_section(const ca::str::Utf8StringRef& section) {
    bool removed = false;
    if (section.is_empty()) {
        records_.erase(
            std::remove_if(records_.begin(), records_.end(), [&](const detail::LineRecord& record) {
                const bool should_remove =
                    record.line.kind == IniLineKind::KeyValue && record.line.section.is_empty();
                removed = removed || should_remove;
                return should_remove;
            }),
            records_.end());
        if (removed) rebuild_index();
        return removed;
    }

    auto it = section_index_.find(section);
    if (it == section_index_.end()) return false;

    const ca::usize start = it->second;
    ca::usize end = start + 1;
    while (end < records_.size() && records_[end].line.kind != IniLineKind::Section) {
        ++end;
    }
    // 回收 section 末尾紧跟的空行/注释。
    if (end < records_.size()) {
        while (end > start + 1) {
            const auto kind = records_[end - 1].line.kind;
            if (kind != IniLineKind::Blank && kind != IniLineKind::Comment) break;
            --end;
        }
    }
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(start),
                   records_.begin() + static_cast<std::ptrdiff_t>(end));
    rebuild_index();
    return true;
}

bool IniDocument::remove(const ca::str::Utf8StringRef& section,
                         const ca::str::Utf8StringRef& key) {
    auto section_it = key_index_.find(section);
    if (section_it == key_index_.end()) return false;
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) return false;
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(key_it->second));
    rebuild_index();
    return true;
}

// ============================================================================
// 枚举
// ============================================================================

std::vector<ca::str::Utf8StringRef> IniDocument::sections() const {
    std::vector<ca::str::Utf8StringRef> result;
    for (const auto& record : records_) {
        if (record.line.kind == IniLineKind::Section) {
            result.push_back(record.line.section);
        }
    }
    return result;
}

std::vector<ca::str::Utf8StringRef> IniDocument::keys(const ca::str::Utf8StringRef& section) const {
    // 用 string_view 做匹配和去重，避免每次循环分配 std::string。
    const std::string_view sec_view(reinterpret_cast<const char*>(section.data()),
                                    section.byte_length());
    std::vector<ca::str::Utf8StringRef> result;
    std::set<std::string_view> seen;
    for (const auto& record : records_) {
        if (record.line.kind != IniLineKind::KeyValue) continue;
        const std::string_view rec_sec(
            reinterpret_cast<const char*>(record.line.section.data()),
            record.line.section.byte_length());
        if (rec_sec != sec_view) continue;
        const std::string_view k_view(reinterpret_cast<const char*>(record.line.key.data()),
                                      record.line.key.byte_length());
        if (seen.insert(k_view).second) {
            result.push_back(record.line.key);
        }
    }
    return result;
}

const std::vector<IniLine>& IniDocument::lines() const noexcept {
    return public_lines_;
}

void IniDocument::clear() noexcept {
    records_.clear();
    public_lines_.clear();
    section_index_.clear();
    key_index_.clear();
    default_line_ending_ = "\n";
    arena_.clear();
}

// ============================================================================
// 内部辅助
// ============================================================================

void IniDocument::add_record(detail::LineRecord record) {
    if (default_line_ending_ == "\n" && !record.line_ending.is_empty()) {
        default_line_ending_ = to_std(record.line_ending);
    }
    records_.push_back(std::move(record));
}

void IniDocument::rebuild_index() {
    public_lines_.clear();
    section_index_.clear();
    key_index_.clear();
    public_lines_.reserve(records_.size());

    for (ca::usize i = 0; i < records_.size(); ++i) {
        const auto& line = records_[i].line;
        IniLine public_line;
        public_line.kind = line.kind;
        public_line.section = line.section;
        public_line.key = line.key;
        public_line.value = line.value;
        public_lines_.push_back(std::move(public_line));

        if (line.kind == IniLineKind::Section) {
            section_index_[line.section] = i;
        } else if (line.kind == IniLineKind::KeyValue) {
            key_index_[line.section][line.key] = i;
        }
    }
}

void IniDocument::rebuild_key_raw(ca::usize line_index) {
    auto& record = records_[line_index];
    // 若 value 原本带引号，重建时补回引号。
    std::string value_str = to_std(record.line.value);
    if (record.value_quoted) {
        value_str = std::string(1, record.value_quote_char) + value_str +
                    std::string(1, record.value_quote_char);
    }
    const std::string rebuilt = to_std(record.key_prefix) + to_std(record.line.key) +
                                to_std(record.key_suffix) + to_std(record.separator) +
                                to_std(record.value_prefix) + value_str +
                                to_std(record.comment_suffix);
    record.raw = intern_std(arena_, rebuilt);
}

ca::usize IniDocument::find_insert_position(const ca::str::Utf8StringRef& section) const noexcept {
    if (section.is_empty()) {
        ca::usize pos = 0;
        while (pos < records_.size() && records_[pos].line.kind != IniLineKind::Section) {
            ++pos;
        }
        return pos;
    }
    auto section_it = section_index_.find(section);
    if (section_it == section_index_.end()) return records_.size();
    ca::usize pos = section_it->second + 1;
    while (pos < records_.size() && records_[pos].line.kind != IniLineKind::Section) {
        ++pos;
    }
    return pos;
}

std::string IniDocument::line_ending_for_new_line() const {
    return default_line_ending_.empty() ? std::string("\n") : default_line_ending_;
}

}  // namespace ca::ini
