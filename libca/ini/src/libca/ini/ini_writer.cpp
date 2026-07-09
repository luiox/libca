#include "libca/ini/ini_writer.hpp"

#include <fstream>

namespace ca::ini {

std::string IniWriter::write(const IniDocument& document) {
    std::string output;
    for (const auto& record : document.records_) {
        output += record.raw;
        output += record.line_ending;
    }
    return output;
}

ca::Result<void, std::string> IniWriter::write_file(
    const std::string& path,
    const IniDocument& document) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        return ca::Err(std::string("failed to open INI file for writing: ") + path);
    }

    output << write(document);
    if (!output.good()) {
        return ca::Err(std::string("failed to write INI file: ") + path);
    }
    return ca::Ok();
}

}  // namespace ca::ini
