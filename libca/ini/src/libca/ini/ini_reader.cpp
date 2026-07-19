#include "libca/ini/ini_reader.hpp"

#include "libca/ini/ini_document.hpp"
#include "libca/ini/parse_error.hpp"
#include "libca/ini/source_location.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace ca::ini {

namespace {

using ca::u8;
using ca::usize;

// Utf8StringRef 与 std::string 互转（内部用 std::string 做字符扫描更直接）。
std::string to_std(const ca::str::Utf8StringRef& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

// 把 std::string 经 arena 入池得到 Utf8StringRef（parse_line 内统一入口）。
ca::str::Utf8StringRef intern_std(ca::str::Utf8StringArena& arena, const std::string& s) {
    return arena.intern(ca::str::Utf8String(
        reinterpret_cast<const ca::u8*>(s.data()), s.size()));
}

bool is_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string trim_ascii(const std::string& value) {
    usize begin = 0;
    usize end = value.size();
    while (begin < end && is_space(value[begin])) ++begin;
    while (end > begin && is_space(value[end - 1])) --end;
    return value.substr(begin, end - begin);
}

bool is_comment_marker(char ch, const IniReaderOptions& options) noexcept {
    return (options.hash_comment && ch == '#') ||
           (options.semicolon_comment && ch == ';');
}

// 在 value 部分中查找行内注释起始位置。考虑引号和转义。
// strict_whitespace=true 时要求注释符前是空白（或位于 value 起始）。
usize find_inline_comment(const std::string& value,
                          const IniReaderOptions& options) {
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;
    for (usize i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '\\' && !escaped) {
            escaped = true;
            continue;
        }
        if (ch == '\'' && !in_double_quote && !escaped) {
            in_single_quote = !in_single_quote;
        } else if (ch == '"' && !in_single_quote && !escaped) {
            in_double_quote = !in_double_quote;
        } else if (!in_single_quote && !in_double_quote && !escaped &&
                   is_comment_marker(ch, options) &&
                   (!options.inline_comment_strict_whitespace ||
                    i == 0 || is_space(value[i - 1]))) {
            return i;
        }
        escaped = false;
    }
    return value.size();
}

// 判断 value 末尾引号是否被反斜杠转义：从末尾引号前一个字节起向前数连续反斜杠，
// 奇数个表示转义（该引号不是闭合引号）。
bool trailing_quote_escaped(const std::string& value) {
    std::size_t backslashes = 0;
    std::size_t i = value.size() - 1;  // 指向末尾引号
    while (i > 0 && value[i - 1] == '\\') {
        ++backslashes;
        --i;
    }
    return (backslashes % 2) != 0;
}

// 判断 value 是否被首尾配对的引号包裹（且末尾引号未被转义），若是返回 true 并写出引号字符。
bool detect_quotes(const std::string& value, char& quote_char) {
    if (value.size() < 2) return false;
    const char first = value.front();
    const char last = value.back();
    if ((first == '"' || first == '\'') && first == last &&
        !trailing_quote_escaped(value)) {
        quote_char = first;
        return true;
    }
    return false;
}

// 解析单行的结果：填充 record、可能切换 section、可能产生错误。
struct LineParse {
    detail::LineRecord record;
    std::string next_section;   // 若该行是 section，写新 section 名
    bool changed_section = false;
    bool error = false;
    ParseError parse_error;
};

void parse_line(ca::str::Utf8StringArena& arena,
                const std::string& raw,
                const std::string& line_ending,
                const std::string& current_section,
                const IniReaderOptions& options,
                usize line_number,
                usize line_start_column,  // 该行第一列的列号（通常 1）
                LineParse& out) {
    out.record.raw = intern_std(arena, raw);
    out.record.line_ending = intern_std(arena, line_ending);
    out.record.line.section = intern_std(arena, current_section);

    const auto trimmed = trim_ascii(raw);
    if (trimmed.empty()) {
        out.record.line.kind = IniLineKind::Blank;
        return;
    }
    if (is_comment_marker(trimmed[0], options)) {
        out.record.line.kind = IniLineKind::Comment;
        return;
    }

    if (trimmed[0] == '[') {
        const auto end = trimmed.find(']');
        if (end == std::string::npos) {
            out.error = true;
            out.parse_error.location = SourceLocation{line_number, line_start_column};
            out.parse_error.message = ca::str::Utf8String::from_cstr("section header misses closing ']'");
            return;
        }
        out.record.line.kind = IniLineKind::Section;
        out.record.line.section = intern_std(arena, trim_ascii(trimmed.substr(1, end - 1)));
        out.next_section = trim_ascii(trimmed.substr(1, end - 1));
        out.changed_section = true;
        return;
    }

    const auto equal_pos = raw.find('=');
    const auto colon_pos = raw.find(':');
    usize separator_pos = std::string::npos;
    if (equal_pos == std::string::npos) {
        separator_pos = colon_pos;
    } else if (colon_pos == std::string::npos) {
        separator_pos = equal_pos;
    } else {
        separator_pos = equal_pos < colon_pos ? equal_pos : colon_pos;
    }

    if (separator_pos == std::string::npos) {
        out.error = true;
        out.parse_error.location = SourceLocation{line_number, line_start_column};
        out.parse_error.message = ca::str::Utf8String::from_cstr("key/value line misses '=' or ':'");
        return;
    }
    if (current_section.empty() && !options.allow_global_keys) {
        out.error = true;
        out.parse_error.location = SourceLocation{line_number, line_start_column};
        out.parse_error.message = ca::str::Utf8String::from_cstr("global key/value is disabled");
        return;
    }

    const auto key_part = raw.substr(0, separator_pos);
    usize key_begin = 0;
    usize key_end = key_part.size();
    while (key_begin < key_end && is_space(key_part[key_begin])) ++key_begin;
    while (key_end > key_begin && is_space(key_part[key_end - 1])) --key_end;

    const auto value_part = raw.substr(separator_pos + 1);
    usize value_begin = 0;
    while (value_begin < value_part.size() && is_space(value_part[value_begin])) ++value_begin;
    const usize comment_pos = find_inline_comment(value_part, options);
    usize value_end = comment_pos;
    while (value_end > value_begin && is_space(value_part[value_end - 1])) --value_end;

    const std::string key_str = key_part.substr(key_begin, key_end - key_begin);
    const std::string value_str = value_part.substr(value_begin, value_end - value_begin);

    out.record.line.kind = IniLineKind::KeyValue;
    out.record.line.section = intern_std(arena, current_section);
    out.record.line.key = intern_std(arena, key_str);
    out.record.line.value = intern_std(arena, value_str);
    out.record.key_prefix = intern_std(arena, key_part.substr(0, key_begin));
    out.record.key_suffix = intern_std(arena, key_part.substr(key_end));
    out.record.separator = intern_std(arena, raw.substr(separator_pos, 1));
    out.record.value_prefix = intern_std(arena, value_part.substr(0, value_begin));
    out.record.comment_suffix = value_end < value_part.size()
                                    ? intern_std(arena, value_part.substr(value_end))
                                    : ca::str::Utf8StringRef();
    // 记录 value 是否带引号（供 set() 重建时补回）。
    char qchar = '"';
    out.record.value_quoted = detect_quotes(value_str, qchar);
    out.record.value_quote_char = qchar;
}

}  // namespace

ca::Result<IniDocument, ParseError> IniReader::read(
    const ca::str::Utf8StringRef& input,
    const IniReaderOptions& options) {
    IniDocument document;
    auto& arena = document.arena();
    std::string current_section;
    usize line_number = 1;

    // 用 std::string 做行扫描（字符级处理更直接），解析后经 arena 入池。
    const std::string text = to_std(input);

    // 重复检测记账
    std::map<std::string, bool> seen_sections;
    std::map<std::string, std::map<std::string, bool>> seen_keys;

    for (usize pos = 0; pos <= text.size();) {
        if (pos == text.size()) break;

        const usize line_start = pos;
        usize line_end = pos;
        while (line_end < text.size() &&
               text[line_end] != '\r' && text[line_end] != '\n') {
            ++line_end;
        }
        const auto raw = text.substr(line_start, line_end - line_start);

        std::string line_ending;
        if (line_end < text.size()) {
            if (text[line_end] == '\r' &&
                line_end + 1 < text.size() && text[line_end + 1] == '\n') {
                line_ending = "\r\n";
                pos = line_end + 2;
            } else {
                line_ending = text.substr(line_end, 1);
                pos = line_end + 1;
            }
        } else {
            pos = line_end;
        }

        LineParse parsed;
        parse_line(arena, raw, line_ending, current_section, options, line_number, 1, parsed);
        if (parsed.error) {
            return ca::Err(std::move(parsed.parse_error));
        }

        // 重复检测：section
        if (parsed.changed_section) {
            current_section = parsed.next_section;
            if (options.on_duplicate_section == DuplicatePolicy::Error &&
                seen_sections.find(current_section) != seen_sections.end()) {
                ParseError err;
                err.location = SourceLocation{line_number, 1};
                err.message = ca::str::Utf8String::from_cstr(
                    ("duplicate section: " + current_section).c_str());
                return ca::Err(std::move(err));
            }
            seen_sections[current_section] = true;
        }

        // 重复检测：key
        if (parsed.record.line.kind == IniLineKind::KeyValue) {
            const std::string& sec = current_section;
            const std::string key_str = to_std(parsed.record.line.key);
            if (options.on_duplicate_key == DuplicatePolicy::Error &&
                seen_keys[sec].find(key_str) != seen_keys[sec].end()) {
                ParseError err;
                err.location = SourceLocation{line_number, 1};
                err.message = ca::str::Utf8String::from_cstr(
                    ("duplicate key '" + key_str + "' in section: " + sec).c_str());
                return ca::Err(std::move(err));
            }
            seen_keys[sec][key_str] = true;
        }

        document.add_record(std::move(parsed.record));
        ++line_number;
    }

    document.rebuild_index();
    return ca::Ok(std::move(document));
}

ca::Result<IniDocument, ParseError> IniReader::read_file(
    const ca::str::Utf8StringRef& path,
    const IniReaderOptions& options) {
    std::string path_str = to_std(path);
    std::ifstream input(path_str, std::ios::binary);
    if (!input.is_open()) {
        ParseError err;
        err.location = SourceLocation{};
        err.message = ca::str::Utf8String::from_cstr(
            ("failed to open INI file: " + path_str).c_str());
        return ca::Err(std::move(err));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();
    // text 是局部变量，read() 同步完成解析，视图在其返回前一直有效。
    return read(ca::str::Utf8StringRef::from_string_view(
        std::string_view(text.data(), text.size())), options);
}

}  // namespace ca::ini
