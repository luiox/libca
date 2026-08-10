#include "libca/ini/ini_writer.hpp"

#include "libca/str/format.hpp"
#include "libca/str/utf8_string.hpp"

#include <fstream>
#include <string>

namespace ca::ini {

namespace {

std::string to_std(const ca::str::Utf8StringRef& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

}  // namespace

ca::str::Utf8String IniWriter::write(const IniDocument& document,
                                     const IniWriterOptions& options) {
    std::string output;
    for (const auto& record : document.records_) {
        output += to_std(record.raw);
        if (options.line_ending.empty()) {
            output += to_std(record.line_ending);
        } else {
            output += options.line_ending;
        }
    }
    return ca::str::Utf8String(reinterpret_cast<const ca::u8*>(output.data()), output.size());
}

ca::Result<void, ca::str::Utf8String> IniWriter::write_file(
    const ca::str::Utf8StringRef& path,
    const IniDocument& document,
    const IniWriterOptions& options) {
    std::string path_str(reinterpret_cast<const char*>(path.data()),
                         reinterpret_cast<const char*>(path.data()) + path.byte_length());
    std::ofstream output(path_str, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ca::str::format_std("failed to open INI file for writing: {}", path_str).c_str()));
    }
    ca::str::Utf8String text = write(document, options);
    output.write(reinterpret_cast<const char*>(text.data()),
                 static_cast<std::streamsize>(text.byte_length()));
    if (!output.good()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ca::str::format_std("failed to write INI file: {}", path_str).c_str()));
    }
    return ca::Ok();
}

}  // namespace ca::ini
