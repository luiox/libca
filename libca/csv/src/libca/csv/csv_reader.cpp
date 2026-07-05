#include "libca/csv/csv_reader.hpp"

#include <fstream>
#include <sstream>

namespace ca::csv {
namespace {

std::string trim_ascii_space(const std::string& value) {
    ca::usize begin = 0;
    ca::usize end = value.size();
    while (begin < end &&
           (value[begin] == ' ' || value[begin] == '\t' ||
            value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }
    while (end > begin &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' ||
            value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string parse_error(ca::usize line, ca::usize column, const char* message) {
    return std::string("CSV parse error at line ") + std::to_string(line) +
           ", column " + std::to_string(column) + ": " + message;
}

}  // namespace

ca::Result<CsvDocument, std::string> CsvReader::read(
    const std::string& text,
    const CsvReaderOptions& options) {
    if (options.delimiter == options.quote) {
        return ca::Err(std::string("CSV delimiter and quote must be different"));
    }

    std::vector<CsvRow> parsed_rows;
    std::vector<std::string> current_row;
    std::string current_field;
    bool in_quotes = false;
    bool field_was_quoted = false;
    bool saw_any_char = false;
    ca::usize line = 1;
    ca::usize column = 1;

    auto finish_field = [&]() {
        if (!field_was_quoted && options.trim_unquoted_space) {
            current_row.push_back(trim_ascii_space(current_field));
        } else {
            current_row.push_back(current_field);
        }
        current_field.clear();
        field_was_quoted = false;
    };

    auto finish_row = [&]() {
        finish_field();
        parsed_rows.emplace_back(std::move(current_row));
        current_row.clear();
    };

    // 单趟状态机解析：非 quoted 状态识别分隔符/行尾，quoted 状态只特殊处理
    // quote 与 quote quote，其余字符（包括换行）都属于字段内容。
    for (ca::usize i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        saw_any_char = true;

        if (in_quotes) {
            if (ch == options.quote) {
                if (i + 1 < text.size() && text[i + 1] == options.quote) {
                    current_field.push_back(options.quote);
                    ++i;
                    ++column;
                } else {
                    in_quotes = false;
                }
            } else {
                current_field.push_back(ch);
                if (ch == '\n') {
                    ++line;
                    column = 0;
                }
            }
            ++column;
            continue;
        }

        if (ch == options.quote && current_field.empty()) {
            in_quotes = true;
            field_was_quoted = true;
        } else if (ch == options.delimiter) {
            finish_field();
        } else if (ch == '\r' || ch == '\n') {
            finish_row();
            if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
            ++line;
            column = 0;
        } else if (field_was_quoted) {
            return ca::Err(parse_error(line, column, "unexpected character after closing quote"));
        } else {
            current_field.push_back(ch);
        }
        ++column;
    }

    if (in_quotes) {
        return ca::Err(parse_error(line, column, "unterminated quoted field"));
    }

    // 文件末尾没有换行时补最后一行；纯空文本保持空文档。
    if (saw_any_char && (!current_field.empty() || !current_row.empty() ||
                         text.back() == options.delimiter)) {
        finish_row();
    }

    CsvDocument document;
    if (options.first_row_is_header && !parsed_rows.empty()) {
        document.set_header(std::move(parsed_rows.front().fields()));
        for (ca::usize i = 1; i < parsed_rows.size(); ++i) {
            document.add_row(std::move(parsed_rows[i]));
        }
    } else {
        for (auto& row : parsed_rows) {
            document.add_row(std::move(row));
        }
    }

    return ca::Ok(std::move(document));
}

ca::Result<CsvDocument, std::string> CsvReader::read_file(
    const std::string& path,
    const CsvReaderOptions& options) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return ca::Err(std::string("failed to open CSV file: ") + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return read(buffer.str(), options);
}

}  // namespace ca::csv
