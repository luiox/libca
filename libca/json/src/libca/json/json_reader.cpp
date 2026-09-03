#include "libca/json/json_reader.hpp"

#include "libca/fs/path.hpp"

#include "libca/json/json_dom_builder.hpp"
#include "libca/json/json_parser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace ca::json {

namespace {

JsonParserOptions to_parser_options(const JsonReaderOptions& o) {
    JsonParserOptions p;
    p.allow_trailing_comma = o.allow_trailing_comma;
    p.allow_comments = o.allow_comments;
    return p;
}

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
    std::string msg = "failed to open JSON file: ";
    msg.append(path.data(), path.data() + path.byte_length());
    err.message = ca::str::Utf8String::from_cstr(msg.c_str());
    return err;
}

}  // namespace

ca::Result<JsonDocument, ParseError> JsonReader::read(
    const ca::str::Utf8StringRef& input,
    const JsonReaderOptions& options) {
    JsonDocument document;
    JsonDomBuilder builder;
    JsonParser parser(input, builder, document.arena(), to_parser_options(options));
    if (!parser.parse()) {
        return ca::Err(clone_error(parser.last_error()));
    }
    if (builder.has_error()) {
        return ca::Err(clone_error(builder.error()));
    }
    document.root() = builder.take_root();
    return ca::Ok(std::move(document));
}

ca::Result<JsonDocument, ParseError> JsonReader::read_file(
    const ca::str::Utf8StringRef& path,
    const JsonReaderOptions& options) {
    // 路径转 std::string（ifstream 需要）
    std::string path_str(path.data(), path.data() + path.byte_length());
    std::ifstream input(ca::fs::Path::from_utf8_lossy(path_str).native(), std::ios::binary);
    if (!input.is_open()) {
        return ca::Err(make_open_error(path));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    // 用值拷贝而非 const& 绑定临时量，避免依赖生命周期延长（脆弱）。NRVO 会消除拷贝。
    std::string text = buffer.str();

    // text 是局部 std::string，Utf8StringRef 在解析期间必须有效。
    // read() 内部同步完成解析，所以 text 在返回前一直有效。
    ca::str::Utf8StringRef view = ca::str::Utf8StringRef::from_string_view(
        std::string_view(text.data(), text.size()));
    return read(view, options);
}

}  // namespace ca::json
