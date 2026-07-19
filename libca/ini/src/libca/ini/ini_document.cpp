#include "libca/ini/ini_document.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace ca::ini {

namespace {

// Utf8StringRef 转 std::string（用于内部索引键）。
std::string to_std(const ca::str::Utf8StringRef& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

// Utf8String 转 std::string。
std::string to_std(const ca::str::Utf8String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

// std::string 转 Utf8String（move 语义）。
ca::str::Utf8String from_std(std::string s) {
    return ca::str::Utf8String::from_cstr(s.c_str());
}

// 剥首尾配对的单/双引号。若 value 以同一个引号字符开头且结尾，去掉两者。
// 返回 Utf8String（持有）；输入视图内容必须是合法 UTF-8。
ca::str::Utf8String strip_quotes(const ca::str::Utf8StringRef& value) {
    const ca::usize blen = value.byte_length();
    if (blen < 2) {
        return ca::str::Utf8String(value.data(), blen);
    }
    const ca::u8 first = value.byte_at(0);
    const ca::u8 last = value.byte_at(blen - 1);
    if ((first == '"' || first == '\'') && first == last) {
        // 去掉首尾各一字节（引号是单字节 ASCII，切片落在码点边界）
        const ca::u8* p = value.data() + 1;
        return ca::str::Utf8String(p, blen - 2);
    }
    return ca::str::Utf8String(value.data(), blen);
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
// 析构（records_ 含 Utf8String，需要完整类型；默认实现即可，此处显式声明便于将来扩展）
// ============================================================================

IniDocument::~IniDocument() = default;

// ============================================================================
// 内部查找
// ============================================================================

const ca::str::Utf8String* IniDocument::find_value(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const std::string sec = to_std(section);
    auto section_it = key_index_.find(sec);
    if (section_it == key_index_.end()) return nullptr;
    auto key_it = section_it->second.find(to_std(key));
    if (key_it == section_it->second.end()) return nullptr;
    return &records_[key_it->second].line.value;
}

// ============================================================================
// 查询
// ============================================================================

bool IniDocument::has_section(const ca::str::Utf8StringRef& section) const {
    const std::string key = to_std(section);
    if (key.empty()) {
        return key_index_.find(key) != key_index_.end();
    }
    return section_index_.find(key) != section_index_.end();
}

bool IniDocument::has(const ca::str::Utf8StringRef& section,
                      const ca::str::Utf8StringRef& key) const {
    const std::string sec = to_std(section);
    auto section_it = key_index_.find(sec);
    if (section_it == key_index_.end()) return false;
    return section_it->second.find(to_std(key)) != section_it->second.end();
}

ca::Result<ca::str::Utf8String, ca::str::Utf8String> IniDocument::get(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const std::string sec = to_std(section);
    auto section_it = key_index_.find(sec);
    if (section_it == key_index_.end()) {
        return ca::Err(from_std("INI section not found: " + to_std(section)));
    }
    auto key_it = section_it->second.find(to_std(key));
    if (key_it == section_it->second.end()) {
        return ca::Err(from_std("INI key not found: " + to_std(key)));
    }
    return ca::Ok(records_[key_it->second].line.value.clone());
}

ca::str::Utf8String IniDocument::get_or(const ca::str::Utf8StringRef& section,
                                        const ca::str::Utf8StringRef& key,
                                        const ca::str::Utf8StringRef& default_value) const {
    const ca::str::Utf8String* value = find_value(section, key);
    if (value != nullptr) {
        return value->clone();
    }
    return default_value.substr(0, default_value.length());
}

// ============================================================================
// 类型化访问
// ============================================================================

ca::Result<ca::i64, ca::str::Utf8String> IniDocument::get_int(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const ca::str::Utf8String* value = find_value(section, key);
    if (value == nullptr) {
        return ca::Err(from_std("INI key not found: " + to_std(key)));
    }
    ca::str::Utf8String stripped = strip_quotes(value->ref());
    std::string s = to_std(stripped);
    try {
        std::size_t pos = 0;
        long long v = std::stoll(s, &pos);
        if (pos != s.size()) {
            return ca::Err(from_std("INI value is not a valid integer: " + to_std(*value)));
        }
        return ca::Ok(static_cast<ca::i64>(v));
    } catch (...) {
        return ca::Err(from_std("INI value is not a valid integer: " + to_std(*value)));
    }
}

ca::Result<ca::f64, ca::str::Utf8String> IniDocument::get_double(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const ca::str::Utf8String* value = find_value(section, key);
    if (value == nullptr) {
        return ca::Err(from_std("INI key not found: " + to_std(key)));
    }
    ca::str::Utf8String stripped = strip_quotes(value->ref());
    std::string s = to_std(stripped);
    try {
        std::size_t pos = 0;
        double v = std::stod(s, &pos);
        if (pos != s.size()) {
            return ca::Err(from_std("INI value is not a valid number: " + to_std(*value)));
        }
        return ca::Ok(static_cast<ca::f64>(v));
    } catch (...) {
        return ca::Err(from_std("INI value is not a valid number: " + to_std(*value)));
    }
}

ca::Result<bool, ca::str::Utf8String> IniDocument::get_bool(
    const ca::str::Utf8StringRef& section,
    const ca::str::Utf8StringRef& key) const {
    const ca::str::Utf8String* value = find_value(section, key);
    if (value == nullptr) {
        return ca::Err(from_std("INI key not found: " + to_std(key)));
    }
    ca::str::Utf8String stripped = strip_quotes(value->ref());
    const std::string s = ascii_lower(to_std(stripped));
    if (s == "true" || s == "yes" || s == "on" || s == "1") return ca::Ok(true);
    if (s == "false" || s == "no" || s == "off" || s == "0") return ca::Ok(false);
    return ca::Err(from_std("INI value is not a valid boolean: " + to_std(*value)));
}

// ============================================================================
// 编辑：set
// ============================================================================

void IniDocument::set(const ca::str::Utf8StringRef& section,
                      const ca::str::Utf8StringRef& key,
                      const ca::str::Utf8StringRef& value) {
    const std::string sec = to_std(section);

    // 已存在的 key：改 value，复用保存的格式片段重建该行。
    auto section_it = key_index_.find(sec);
    if (section_it != key_index_.end()) {
        auto key_it = section_it->second.find(to_std(key));
        if (key_it != section_it->second.end()) {
            auto& record = records_[key_it->second];
            record.line.value = value.substr(0, value.length());
            rebuild_key_raw(key_it->second);
            // 同步更新 public_lines_（Utf8String 不可拷贝，逐字段 clone）。
            IniLine updated;
            updated.kind = record.line.kind;
            updated.section = record.line.section.clone();
            updated.key = record.line.key.clone();
            updated.value = record.line.value.clone();
            public_lines_[key_it->second] = std::move(updated);
            return;
        }
    }

    const auto newline = line_ending_for_new_line();

    // 新 key 落在尚不存在的 section：先建 section 头。
    if (!sec.empty() && section_index_.find(sec) == section_index_.end()) {
        if (!records_.empty() && !records_.back().raw.is_empty()) {
            detail::LineRecord blank;
            blank.line.kind = IniLineKind::Blank;
            blank.line_ending = from_std(newline);
            records_.push_back(std::move(blank));
        }

        detail::LineRecord section_record;
        section_record.line.kind = IniLineKind::Section;
        section_record.line.section = section.substr(0, section.length());
        section_record.raw = from_std("[" + sec + "]");
        section_record.line_ending = from_std(newline);
        records_.push_back(std::move(section_record));
        rebuild_index();
    }

    // 新增 key 行用统一的 "key = value" 格式。
    detail::LineRecord key_record;
    key_record.line.kind = IniLineKind::KeyValue;
    key_record.line.section = section.substr(0, section.length());
    key_record.line.key = key.substr(0, key.length());
    key_record.line.value = value.substr(0, value.length());
    key_record.key_suffix = from_std(" ");
    key_record.separator = from_std("=");
    key_record.value_prefix = from_std(" ");
    key_record.line_ending = from_std(newline);
    key_record.value_quoted = false;
    key_record.raw = from_std(to_std(key) + " = " + to_std(value));

    auto pos = find_insert_position(sec);
    records_.insert(records_.begin() + static_cast<std::ptrdiff_t>(pos),
                    std::move(key_record));
    rebuild_index();
}

// ============================================================================
// 编辑：remove
// ============================================================================

bool IniDocument::remove_section(const ca::str::Utf8StringRef& section) {
    const std::string sec = to_std(section);
    bool removed = false;
    if (sec.empty()) {
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

    auto it = section_index_.find(sec);
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
    const std::string sec = to_std(section);
    auto section_it = key_index_.find(sec);
    if (section_it == key_index_.end()) return false;
    auto key_it = section_it->second.find(to_std(key));
    if (key_it == section_it->second.end()) return false;
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(key_it->second));
    rebuild_index();
    return true;
}

// ============================================================================
// 枚举
// ============================================================================

std::vector<ca::str::Utf8String> IniDocument::sections() const {
    std::vector<ca::str::Utf8String> result;
    for (const auto& record : records_) {
        if (record.line.kind == IniLineKind::Section) {
            result.push_back(record.line.section.clone());
        }
    }
    return result;
}

std::vector<ca::str::Utf8String> IniDocument::keys(const ca::str::Utf8StringRef& section) const {
    const std::string sec = to_std(section);
    std::vector<ca::str::Utf8String> result;
    std::map<std::string, bool> seen;
    for (const auto& record : records_) {
        if (record.line.kind == IniLineKind::KeyValue &&
            to_std(record.line.section.ref()) == sec) {
            const std::string k = to_std(record.line.key.ref());
            if (seen.find(k) == seen.end()) {
                seen[k] = true;
                result.push_back(record.line.key.clone());
            }
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
        public_line.section = line.section.clone();
        public_line.key = line.key.clone();
        public_line.value = line.value.clone();
        public_lines_.push_back(std::move(public_line));

        const std::string sec = to_std(line.section.ref());
        if (line.kind == IniLineKind::Section) {
            section_index_[sec] = i;
        } else if (line.kind == IniLineKind::KeyValue) {
            key_index_[sec][to_std(line.key.ref())] = i;
        }
    }
}

void IniDocument::rebuild_key_raw(ca::usize line_index) {
    auto& record = records_[line_index];
    // 修复 C：若 value 原本带引号，重建时补回引号。
    std::string value_str = to_std(record.line.value);
    if (record.value_quoted) {
        value_str = std::string(1, record.value_quote_char) + value_str +
                    std::string(1, record.value_quote_char);
    }
    record.raw = from_std(to_std(record.key_prefix) + to_std(record.line.key) +
                          to_std(record.key_suffix) + to_std(record.separator) +
                          to_std(record.value_prefix) + value_str +
                          to_std(record.comment_suffix));
}

ca::usize IniDocument::find_insert_position(const std::string& section) const noexcept {
    if (section.empty()) {
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
