#include "libca/csv/csv_writer.hpp"

#include <fstream>
#include <ostream>
#include <sstream>

namespace ca::csv {
namespace {

bool needs_quote(const std::string& field, const CsvWriterOptions& options) {
    if (options.always_quote) {
        return true;
    }
    if (!field.empty() && (field.front() == ' ' || field.back() == ' ' ||
                           field.front() == '\t' || field.back() == '\t')) {
        return true;
    }
    for (char ch : field) {
        if (ch == options.delimiter || ch == options.quote ||
            ch == '\r' || ch == '\n') {
            return true;
        }
    }
    return false;
}

void write_field(std::ostream& output,
                 const std::string& field,
                 const CsvWriterOptions& options) {
    const bool quoted = needs_quote(field, options);
    if (!quoted) {
        output << field;
        return;
    }

    output << options.quote;
    for (char ch : field) {
        if (ch == options.quote) {
            output << options.quote;
        }
        output << ch;
    }
    output << options.quote;
}

void write_row(std::ostream& output,
               const std::vector<std::string>& fields,
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

std::string CsvWriter::write(
    const CsvDocument& document,
    const CsvWriterOptions& options) {
    std::ostringstream output;
    write_to_stream(output, document, options);
    return output.str();
}

ca::Result<void, std::string> CsvWriter::write_file(
    const std::string& path,
    const CsvDocument& document,
    const CsvWriterOptions& options) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        return ca::Err(std::string("failed to open CSV file for writing: ") + path);
    }

    write_to_stream(output, document, options);
    if (!output.good()) {
        return ca::Err(std::string("failed to write CSV file: ") + path);
    }
    return ca::Ok();
}

}  // namespace ca::csv
