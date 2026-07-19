#pragma once

/// @file json_reader.hpp
/// @brief JSON DOM 入口：JsonReader。把字符串/文件解析为 JsonValue。
/// @details 内部用 JsonParser + JsonDomBuilder。需要 SAX 流式处理的用户应直接用 JsonParser，
///          不经过 JsonReader。

#include "libca/core/result.hpp"

#include "libca/json/json_value.hpp"
#include "libca/json/parse_error.hpp"
#include "libca/json/source_location.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::json {

/// @brief JSON 解析选项（DOM 入口）。
struct JsonReaderOptions {
    /// 允许尾随逗号。
    bool allow_trailing_comma = false;
    /// 允许 `//` 和 `/* */` 注释。
    bool allow_comments = false;
};

/// @brief JSON DOM 读取器。
class JsonReader {
public:
    /// @brief 从字符串解析 JSON 为 JsonValue。
    /// @param input JSON 文本视图（须在使用期内有效）。
    /// @param options 解析选项。
    /// @return 成功返回 JsonValue；失败返回 ParseError。
    static ca::Result<JsonValue, ParseError> read(
        const ca::str::Utf8StringRef& input,
        const JsonReaderOptions& options = JsonReaderOptions());

    /// @brief 从文件解析 JSON 为 JsonValue。
    /// @param path 文件路径。
    /// @param options 解析选项。
    /// @return 成功返回 JsonValue；打开失败或格式错误返回 ParseError。
    static ca::Result<JsonValue, ParseError> read_file(
        const ca::str::Utf8StringRef& path,
        const JsonReaderOptions& options = JsonReaderOptions());
};

}  // namespace ca::json
