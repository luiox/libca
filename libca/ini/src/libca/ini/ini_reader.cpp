#include "libca/ini/ini_reader.hpp"

#include <fstream>
#include <sstream>

namespace ca::ini {
namespace {

bool is_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string trim_ascii(const std::string& value) {
    ca::usize begin = 0;
    ca::usize end = value.size();
    while (begin < end && is_space(value[begin])) {
        ++begin;
    }
    while (end > begin && is_space(value[end - 1])) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool is_comment_marker(char ch, const IniReaderOptions& options) noexcept {
    return (options.hash_comment && ch == '#') ||
           (options.semicolon_comment && ch == ';');
}

std::string parse_error(ca::usize line, const char* message) {
    return std::string("INI parse error at line ") + std::to_string(line) +
           ": " + message;
}

ca::usize find_inline_comment(const std::string& value,
                              const IniReaderOptions& options) {
    bool in_single_quote = false;
    bool in_double_quote = false;
    for (ca::usize i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (ch == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (!in_single_quote && !in_double_quote &&
                   is_comment_marker(ch, options) &&
                   (i == 0 || is_space(value[i - 1]))) {
            return i;
        }
    }
    return value.size();
}

IniDocument::LineRecord parse_line(const std::string& raw,
                                   const std::string& line_ending,
                                   const std::string& current_section,
                                   const IniReaderOptions& options,
                                   ca::usize line_number,
                                   std::string* next_section,
                                   std::string* error) {
    IniDocument::LineRecord record;
    record.raw = raw;
    record.line_ending = line_ending;
    record.line.section = current_section;

    const auto trimmed = trim_ascii(raw);
    if (trimmed.empty()) {
        record.line.kind = IniLineKind::Blank;
        return record;
    }
    if (is_comment_marker(trimmed[0], options)) {
        record.line.kind = IniLineKind::Comment;
        return record;
    }

    if (trimmed[0] == '[') {
        const auto end = trimmed.find(']');
        if (end == std::string::npos) {
            *error = parse_error(line_number, "section header misses closing ']'");
            return record;
        }
        record.line.kind = IniLineKind::Section;
        record.line.section = trim_ascii(trimmed.substr(1, end - 1));
        *next_section = record.line.section;
        return record;
    }

    const auto equal_pos = raw.find('=');
    const auto colon_pos = raw.find(':');
    auto separator_pos = std::string::npos;
    if (equal_pos == std::string::npos) {
        separator_pos = colon_pos;
    } else if (colon_pos == std::string::npos) {
        separator_pos = equal_pos;
    } else {
        separator_pos = equal_pos < colon_pos ? equal_pos : colon_pos;
    }

    if (separator_pos == std::string::npos) {
        *error = parse_error(line_number, "key/value line misses '=' or ':'");
        return record;
    }
    if (current_section.empty() && !options.allow_global_keys) {
        *error = parse_error(line_number, "global key/value is disabled");
        return record;
    }

    const auto key_part = raw.substr(0, separator_pos);
    ca::usize key_begin = 0;
    ca::usize key_end = key_part.size();
    while (key_begin < key_end && is_space(key_part[key_begin])) {
        ++key_begin;
    }
    while (key_end > key_begin && is_space(key_part[key_end - 1])) {
        --key_end;
    }

    const auto value_part = raw.substr(separator_pos + 1);
    ca::usize value_begin = 0;
    while (value_begin < value_part.size() && is_space(value_part[value_begin])) {
        ++value_begin;
    }
    const auto comment_pos = find_inline_comment(value_part, options);
    ca::usize value_end = comment_pos;
    while (value_end > value_begin && is_space(value_part[value_end - 1])) {
        --value_end;
    }

    // key/value 行拆成格式片段保存；后续 set() 只替换 value，
    // 仍能保留缩进、分隔符、分隔符后空白和行内注释。
    record.line.kind = IniLineKind::KeyValue;
    record.line.section = current_section;
    record.line.key = key_part.substr(key_begin, key_end - key_begin);
    record.line.value = value_part.substr(value_begin, value_end - value_begin);
    record.key_prefix = key_part.substr(0, key_begin);
    record.key_suffix = key_part.substr(key_end);
    record.separator = raw.substr(separator_pos, 1);
    record.value_prefix = value_part.substr(0, value_begin);
    record.comment_suffix = value_end < value_part.size()
                                ? value_part.substr(value_end)
                                : std::string();
    return record;
}

}  // namespace

ca::Result<IniDocument, std::string> IniReader::read(
    const std::string& text,
    const IniReaderOptions& options) {
    IniDocument document;
    std::string current_section;
    ca::usize line_number = 1;

    for (ca::usize pos = 0; pos <= text.size();) {
        if (pos == text.size()) {
            break;
        }

        const ca::usize line_start = pos;
        ca::usize line_end = pos;
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

        std::string next_section = current_section;
        std::string error;
        auto record = parse_line(raw,
                                 line_ending,
                                 current_section,
                                 options,
                                 line_number,
                                 &next_section,
                                 &error);
        if (!error.empty()) {
            return ca::Err(error);
        }
        document.add_record(std::move(record));
        current_section = next_section;
        ++line_number;
    }

    return ca::Ok(std::move(document));
}

ca::Result<IniDocument, std::string> IniReader::read_file(
    const std::string& path,
    const IniReaderOptions& options) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return ca::Err(std::string("failed to open INI file: ") + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return read(buffer.str(), options);
}

}  // namespace ca::ini
