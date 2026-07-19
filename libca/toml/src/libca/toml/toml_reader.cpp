#include "libca/toml/toml_reader.hpp"

#include "libca/toml/toml_parser.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace ca::toml {

namespace {

// 从 const ParseError ref 构造一个可被 Result move 的副本
// （ParseError 内含 Utf8String，不可拷贝，只能 clone message 后重建）。
ParseError clone_error(const ParseError& src) {
    ParseError dst;
    dst.location = src.location;
    dst.message = src.message.clone();
    return dst;
}

ParseError make_open_error(const ca::str::Utf8StringRef& path) {
    ParseError err;
    err.location = SourceLocation{};
    std::string msg = "failed to open TOML file: ";
    msg.append(path.data(), path.data() + path.byte_length());
    err.message = ca::str::Utf8String::from_cstr(msg.c_str());
    return err;
}

}  // namespace

ca::Result<TomlDocument, ParseError> TomlReader::read(
    const ca::str::Utf8StringRef& input,
    const TomlReaderOptions& /*options*/) {
    TomlDocument document;
    TomlParser parser(document, input);
    if (!parser.run()) {
        return ca::Err(clone_error(parser.last_error()));
    }
    return ca::Ok(std::move(document));
}

ca::Result<TomlDocument, ParseError> TomlReader::read_file(
    const ca::str::Utf8StringRef& path,
    const TomlReaderOptions& options) {
    // 路径转 std::string（ifstream 需要）
    std::string path_str(path.data(), path.data() + path.byte_length());
    std::ifstream input(path_str, std::ios::binary);
    if (!input.is_open()) {
        return ca::Err(make_open_error(path));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string text = buffer.str();

    // text 是局部 std::string，Utf8StringRef 在解析期间必须有效。
    // read() 内部同步完成解析，所以 text 在返回前一直有效。
    ca::str::Utf8StringRef view = ca::str::Utf8StringRef::from_string_view(
        std::string_view(text.data(), text.size()));
    return read(view, options);
}

}  // namespace ca::toml
