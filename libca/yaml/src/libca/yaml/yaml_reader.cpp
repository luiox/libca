#include "libca/yaml/yaml_reader.hpp"

#include "libca/fs/path.hpp"
#include "libca/fs/stdio_file.hpp"

#include "libca/yaml/yaml_parser.hpp"

#include "libca/str/format.hpp"

#include <filesystem>
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

ParseError make_read_error(const ca::str::Utf8StringRef& path) {
    ParseError error;
    error.message = ca::str::Utf8String::from_cstr(
        ca::str::format_std("failed to read YAML file: {}", path).c_str());
    return error;
}

// Reader 选项 → parser 选项（对齐 xml_reader 的 to_parser_options 模式）。
YamlParserOptions to_parser_options(const YamlReaderOptions& o) {
    YamlParserOptions p;
    p.max_depth = o.max_depth;
    return p;
}

}  // namespace

ca::Result<YamlDocument, ParseError> YamlReader::read(const ca::str::Utf8StringRef& input,
                                                      const YamlReaderOptions& options) {
    YamlDocument document;
    YamlParser parser(document, input, to_parser_options(options));
    if (!parser.run()) {
        return ca::Err(clone_error(parser.last_error()));
    }
    return ca::Ok(std::move(document));
}

ca::Result<YamlDocument, ParseError> YamlReader::read_file(const ca::str::Utf8StringRef& path,
                                                           const YamlReaderOptions& options) {
    // 整读经 fs 的 stdio 路径（#249：ifstream+ostringstream 双层流换 fopen+fread 单读）
    std::string path_str(reinterpret_cast<const char*>(path.data()),
                         reinterpret_cast<const char*>(path.data()) + path.byte_length());
    auto content = ca::fs::read_file_all(ca::fs::Path::from_utf8_lossy(path_str));
    if (content.is_err()) {
        return ca::Err(make_read_error(path));
    }
    std::string text = std::move(content).unwrap();
    // 解析是同步的，text 覆盖 read() 全程，零拷贝视图安全。
    return read(ca::str::Utf8StringRef::from_string_view(std::string_view(text.data(), text.size())),
                options);
}

}  // namespace ca::yaml
