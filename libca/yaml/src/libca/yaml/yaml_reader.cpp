#include "libca/yaml/yaml_reader.hpp"

#include "libca/yaml/yaml_parser.hpp"

#include "libca/str/format.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace ca::yaml {

namespace {

// Utf8String 是 move-only，last_error() 给的是 const 引用，须 clone 出可移动副本。
ParseError clone_error(const ParseError& error) {
    ParseError copy;
    copy.location = error.location;
    copy.message = error.message.clone();
    return copy;
}

ParseError make_open_error(const ca::str::Utf8StringRef& path) {
    ParseError error;
    error.message = ca::str::Utf8String::from_cstr(
        ca::str::format_std("failed to open YAML file: {}", path).c_str());
    return error;
}

}  // namespace

ca::Result<YamlDocument, ParseError> YamlReader::read(const ca::str::Utf8StringRef& input,
                                                      const YamlReaderOptions& options) {
    (void)options;
    YamlDocument document;
    YamlParser parser(document, input);
    if (!parser.run()) {
        return ca::Err(clone_error(parser.last_error()));
    }
    return ca::Ok(std::move(document));
}

ca::Result<YamlDocument, ParseError> YamlReader::read_file(const ca::str::Utf8StringRef& path,
                                                           const YamlReaderOptions& options) {
    std::string path_str(reinterpret_cast<const char*>(path.data()),
                         reinterpret_cast<const char*>(path.data()) + path.byte_length());
    std::ifstream input(std::filesystem::u8path(path_str), std::ios::binary);
    if (!input.is_open()) {
        return ca::Err(make_open_error(path));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();
    // 解析是同步的，text 覆盖 read() 全程，零拷贝视图安全。
    return read(ca::str::Utf8StringRef::from_string_view(std::string_view(text.data(), text.size())),
                options);
}

}  // namespace ca::yaml
