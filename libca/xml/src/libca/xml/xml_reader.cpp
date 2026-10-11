#include "libca/xml/xml_reader.hpp"

#include "libca/fs/path.hpp"
#include "libca/fs/stdio_file.hpp"

#include "libca/xml/xml_parser.hpp"

#include <filesystem>
#include <string>

namespace ca::xml {

namespace {

// 从 const ParseError ref 构造一个可被 Result move 的副本
// （ParseError 内含 Utf8String，不可拷贝，只能 clone message 后重建）。
ParseError clone_error(const ParseError& src) {
    ParseError dst;
    dst.location = src.location;
    dst.message = src.message.clone();
    return dst;
}

ParseError make_read_error(const ca::str::Utf8StringRef& path) {
    ParseError err;
    err.location = SourceLocation{};
    std::string msg = "failed to read XML file: ";
    msg.append(path.data(), path.data() + path.byte_length());
    err.message = ca::str::Utf8String::from_cstr(msg.c_str());
    return err;
}

XmlParserOptions to_parser_options(const XmlReaderOptions& o) {
    XmlParserOptions p;
    p.trim_whitespace = o.trim_whitespace;
    p.max_depth = o.max_depth;
    return p;
}

}  // namespace

ca::Result<XmlDocument, ParseError> XmlReader::read(const ca::str::Utf8StringRef& input,
                                                    const XmlReaderOptions& options) {
    XmlDocument document;
    XmlParser parser(document, input, to_parser_options(options));
    if (!parser.run()) {
        return ca::Err(clone_error(parser.last_error()));
    }
    return ca::Ok(std::move(document));
}

ca::Result<XmlDocument, ParseError> XmlReader::read_file(const ca::str::Utf8StringRef& path,
                                                         const XmlReaderOptions& options) {
    // 整读经 fs 的 stdio 路径（#249：ifstream+ostringstream 双层流换 fopen+fread 单读）
    std::string path_str(path.data(), path.data() + path.byte_length());
    auto content = ca::fs::read_file_all(ca::fs::Path::from_utf8_lossy(path_str));
    if (content.is_err()) {
        return ca::Err(make_read_error(path));
    }
    std::string text = std::move(content).unwrap();

    // text 是局部 std::string，Utf8StringRef 在解析期间必须有效。
    // read() 内部同步完成解析，所以 text 在返回前一直有效。
    ca::str::Utf8StringRef view =
        ca::str::Utf8StringRef::from_string_view(std::string_view(text.data(), text.size()));
    return read(view, options);
}

}  // namespace ca::xml
