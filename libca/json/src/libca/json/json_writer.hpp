#pragma once

/// @file json_writer.hpp
/// @brief JSON 序列化器：JsonWriter。把 JsonValue 写为 Utf8String。
/// @details 严格输出符合 RFC 8259 的 JSON。字符串中的控制字符与非 ASCII（可选）转义为
///          `\"` `\\` `\/` `\b` `\f` `\n` `\r` `\t` `\uXXXX`。pretty 模式按缩进换行。

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/json/json_value.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::json {

/// @brief JSON 序列化选项。
struct JsonWriterOptions {
    /// 是否启用美化输出（换行 + 缩进）。
    bool pretty = false;
    /// pretty 模式每级缩进的空格数。
    ca::usize indent = 2;
    /// 是否把所有非 ASCII 字符转义为 `\uXXXX`（输出纯 ASCII）。
    bool ensure_ascii = false;
};

/// @brief JSON 序列化器。
class JsonWriter {
public:
    /// @brief 把 JsonValue 序列化为 Utf8String。
    static ca::str::Utf8String write(const JsonValue& value,
                                     const JsonWriterOptions& options = JsonWriterOptions());

    /// @brief 把 JsonValue 写入文件。
    /// @return 成功返回 Ok；写失败返回错误说明 Utf8String。
    static ca::Result<void, ca::str::Utf8String> write_file(
        const ca::str::Utf8StringRef& path,
        const JsonValue& value,
        const JsonWriterOptions& options = JsonWriterOptions());
};

}  // namespace ca::json
