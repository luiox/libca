#include "libca/csv/csv_reader.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

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

// 构造 ParseError（line/column + 消息）。
ParseError make_error(ca::usize line, ca::usize column, const std::string& message) {
    ParseError err;
    err.location = SourceLocation{line, column};
    err.message = ca::str::Utf8String(reinterpret_cast<const ca::u8*>(message.data()),
                                      message.size());
    return err;
}

}  // namespace

ca::Result<CsvDocument, ParseError> CsvReader::read(
    const ca::str::Utf8StringRef& input,
    const CsvReaderOptions& options) {
    if (options.delimiter == options.quote) {
        return ca::Err(make_error(1, 1, "CSV delimiter and quote must be different"));
    }

    // 内部用 std::string 做字符扫描（CSV 字段不一定是合法 UTF-8，按字节处理更稳妥）。
    const std::string text(reinterpret_cast<const char*>(input.data()),
                           reinterpret_cast<const char*>(input.data()) + input.byte_length());

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
            current_row.push_back(std::move(current_field));
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
            return ca::Err(make_error(line, column,
                                      "unexpected character after closing quote"));
        } else {
            current_field.push_back(ch);
        }
        ++column;
    }

    if (in_quotes) {
        return ca::Err(make_error(line, column, "unterminated quoted field"));
    }

    // 文件末尾没有换行时补最后一行；被引号包裹的空字段也应形成一行。
    if (saw_any_char && (!current_field.empty() || !current_row.empty() ||
                         (!text.empty() && text.back() == options.delimiter) ||
                         field_was_quoted)) {
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

ca::Result<CsvDocument, ParseError> CsvReader::read_file(
    const ca::str::Utf8StringRef& path,
    const CsvReaderOptions& options) {
    std::string path_str(reinterpret_cast<const char*>(path.data()),
                         reinterpret_cast<const char*>(path.data()) + path.byte_length());
    std::ifstream input(path_str, std::ios::binary);
    if (!input.is_open()) {
        return ca::Err(make_error(1, 1, "failed to open CSV file: " + path_str));
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string text = buffer.str();
    // text 是局部 std::string，Utf8StringRef 在解析期间必须有效；read() 同步完成。
    return read(ca::str::Utf8StringRef::from_string_view(
        std::string_view(text.data(), text.size())), options);
}

}  // namespace ca::csv
