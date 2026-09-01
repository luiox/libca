#pragma once

/// @file json_reader.hpp
/// @brief JSON DOM 入口：JsonReader。把字符串/文件解析为 JsonDocument。
/// @details 内部用 JsonParser + JsonDomBuilder。需要 SAX 流式处理的用户应直接用 JsonParser，
///          不经过 JsonReader。

#include "libca/core/result.hpp"

#include "libca/json/json_document.hpp"
#include "libca/json/parse_error.hpp"
#include "libca/json/source_location.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::json {

/// @brief JSON 解析选项（DOM 入口）。
struct JsonReaderOptions
{
    /// 允许尾随逗号。
    bool allow_trailing_comma = false;
    /// 允许 `//` 和 `/* */` 注释。
    bool allow_comments = false;
};

/// @brief JSON DOM 读取器。
class JsonReader
{
public:
    /// @brief 从字符串解析 JSON 为 JsonDocument。
    /// @param input JSON 文本视图（须在使用期内有效）。
    /// @param options 解析选项。
    /// @return 成功返回 JsonDocument（含 arena + root）；失败返回 ParseError。
    ///         返回的 document 内 Utf8StringRef 与输入视图生命周期解耦（已 intern 入池）。
    /// @note 重复 key 语义：对象内出现同名 key 时，**保序保留全部**成员，不做去重，
    ///       `JsonValue::find()` 返回首个匹配。RFC 8259 未规定重复 key 处理，本库选择
    ///       首个优先且不丢弃后续副本。（此前实现为后者覆盖前者，为消除 O(n^2) 装配已调整。）
    static ca::Result<JsonDocument, ParseError> read(
        const ca::str::Utf8StringRef& input,
        const JsonReaderOptions&      options = JsonReaderOptions());

    /// @brief 从文件解析 JSON 为 JsonDocument。
    /// @param path 文件路径。
    /// @param options 解析选项。
    /// @return 成功返回 JsonDocument；打开失败或格式错误返回 ParseError。
    /// @note 重复 key 语义同 read()：保序保留全部，find() 返回首个。
    static ca::Result<JsonDocument, ParseError> read_file(
        const ca::str::Utf8StringRef& path, const JsonReaderOptions& options = JsonReaderOptions());
};

}   // namespace ca::json
