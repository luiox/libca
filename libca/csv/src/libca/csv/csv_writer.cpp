#include "libca/csv/csv_writer.hpp"

#include <fstream>
#include <ostream>
#include <sstream>
#include <string>

namespace ca::csv {
namespace {

// 按 Utf8StringRef 的字节区间判断字段是否需要加引号。
bool needs_quote(const ca::str::Utf8StringRef& field, const CsvWriterOptions& options) {
    if (options.always_quote) {
        return true;
    }
    const ca::usize len = field.byte_length();
    if (len == 0) {
        return false;
    }
    const ca::u8* data = field.data();
    const ca::u8 first = data[0];
    const ca::u8 last = data[len - 1];
    if (first == ' ' || first == '\t' || last == ' ' || last == '\t') {
        return true;
    }
    for (ca::usize i = 0; i < len; ++i) {
        const char ch = static_cast<char>(data[i]);
        if (ch == options.delimiter || ch == options.quote ||
            ch == '\r' || ch == '\n') {
            return true;
        }
    }
    return false;
}

void write_field(std::ostream& output,
                 const ca::str::Utf8StringRef& field,
                 const CsvWriterOptions& options) {
    const bool quoted = needs_quote(field, options);
    if (!quoted) {
        output.write(reinterpret_cast<const char*>(field.data()),
                     static_cast<std::streamsize>(field.byte_length()));
        return;
    }

    output << options.quote;
    const ca::u8* data = field.data();
    const ca::usize len = field.byte_length();
    for (ca::usize i = 0; i < len; ++i) {
        const char ch = static_cast<char>(data[i]);
        if (ch == options.quote) {
            output << options.quote;
        }
        output << ch;
    }
    output << options.quote;
}

void write_row(std::ostream& output,
               const std::vector<ca::str::Utf8StringRef>& fields,
               const CsvWriterOptions& options) {
    for (ca::usize i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            output << options.delimiter;
        }
        write_field(output, fields[i], options);
    }
}

void write_to_stream(std::ostream& output,
                     const CsvDocument& document,
                     const CsvWriterOptions& options) {
    bool wrote_row = false;

    if (options.write_header && document.has_header()) {
        write_row(output, document.header(), options);
        wrote_row = true;
    }

    for (const auto& row : document.rows()) {
        if (wrote_row) {
            output << options.line_ending;
        }
        write_row(output, row.fields(), options);
        wrote_row = true;
    }
}

}  // namespace

ca::str::Utf8String CsvWriter::write(
    const CsvDocument& document,
    const CsvWriterOptions& options) {
    std::ostringstream output;
    write_to_stream(output, document, options);
    const std::string text = output.str();
    if (options.validate_utf8) {
        return ca::str::Utf8String(reinterpret_cast<const ca::u8*>(text.data()), text.size());
    }
    // 字段含非 UTF-8 字节时绕过校验，按原始字节输出（与 CsvDocument::intern_raw 语义对齐）。
    return ca::str::Utf8String::from_data_unchecked(
        reinterpret_cast<const ca::u8*>(text.data()), text.size());
}

ca::Result<void, ca::str::Utf8String> CsvWriter::write_file(
    const ca::str::Utf8StringRef& path,
    const CsvDocument& document,
    const CsvWriterOptions& options) {
    std::string path_str(reinterpret_cast<const char*>(path.data()),
                         reinterpret_cast<const char*>(path.data()) + path.byte_length());
    std::ofstream output(path_str, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("failed to open CSV file for writing: " + path_str).c_str()));
    }

    write_to_stream(output, document, options);
    if (!output.good()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ("failed to write CSV file: " + path_str).c_str()));
    }
    return ca::Ok();
}

}  // namespace ca::csv
