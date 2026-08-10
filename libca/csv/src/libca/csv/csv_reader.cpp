#include "libca/csv/csv_reader.hpp"

#include "libca/str/format.hpp"

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

    CsvDocument document;
    auto& arena = document.arena();

    // 字段收集用 Utf8StringRef vector，每个字段经 intern_raw 入池（不校验 UTF-8，
    // 按原始字节保留——CSV 字段可能是任意字节序列）。
    auto intern_field = [&](const std::string& s) {
        return arena.intern_raw(reinterpret_cast<const ca::u8*>(s.data()), s.size());
    };

    std::vector<CsvRow> parsed_rows;
    std::vector<ca::str::Utf8StringRef> current_row;
    std::string current_field;
    bool in_quotes = false;
    bool field_was_quoted = false;
    bool saw_any_char = false;
    ca::usize line = 1;
    ca::usize column = 1;

    auto finish_field = [&]() {
        std::string value;
        if (!field_was_quoted && options.trim_unquoted_space) {
            value = trim_ascii_space(current_field);
        } else {
            value = std::move(current_field);
        }
        current_row.push_back(intern_field(value));
        current_field.clear();
        field_was_quoted = false;
    };

    auto finish_row = [&]() {
        finish_field();
        parsed_rows.emplace_back(std::move(current_row));
        current_row.clear();
    };

    // 直接对 input 视图做单趟字节扫描（CSV 字段不保证 UTF-8，按字节处理更稳妥）。
    const ca::u8* const data = input.data();
    const ca::usize size = input.byte_length();

    for (ca::usize i = 0; i < size; ++i) {
        const char ch = static_cast<char>(data[i]);
        saw_any_char = true;

        if (in_quotes) {
            if (ch == options.quote) {
                if (i + 1 < size && static_cast<char>(data[i + 1]) == options.quote) {
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
            if (ch == '\r' && i + 1 < size && static_cast<char>(data[i + 1]) == '\n') {
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
                         (size > 0 && static_cast<char>(data[size - 1]) == options.delimiter) ||
                         field_was_quoted)) {
        finish_row();
    }

    if (options.first_row_is_header && !parsed_rows.empty()) {
        // header 字段已 intern 入池，直接接管（header() 的非 const 重载会置 header_enabled_=true）。
        document.header() = std::move(parsed_rows.front().fields());
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
        return ca::Err(make_error(1, 1,
            ca::str::format_std("failed to open CSV file: {}", path_str)));
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string text = buffer.str();
    // text 是局部 std::string，Utf8StringRef 在解析期间必须有效；read() 同步完成。
    return read(ca::str::Utf8StringRef::from_string_view(
        std::string_view(text.data(), text.size())), options);
}

}  // namespace ca::csv
