#pragma once

/// @file toml_writer.hpp
/// @brief TOML 序列化器：TomlWriter。把 TomlDocument 写为 Utf8String。
/// @details 输出符合 TOML 1.0 规范的文本：顶层 key=value 行在前，子表（含数组表）以
///          `[a.b]` / `[[a.b]]` 表头分段在后。字符串默认用 basic string 加必要转义；
///          datetime 按 RFC 3339/ISO 8601 格式输出。

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/toml/toml_document.hpp"

namespace ca::toml {

/// @brief TOML 序列化选项。
struct TomlWriterOptions {
    /// 子表是否缩进（美观）。false 时所有表头都顶格。
    bool indent_subtables = false;
    /// 缩进空格数（indent_subtables=true 时生效）。
    ca::usize indent = 2;
};

/// @brief TOML 序列化器。
class TomlWriter {
public:
    /// @brief 把 TomlDocument 序列化为 Utf8String。
    static ca::str::Utf8String write(
        const TomlDocument& document,
        const TomlWriterOptions& options = TomlWriterOptions());

    /// @brief 把 TomlDocument 写入文件。
    /// @return 成功返回 Ok；写失败返回错误说明 Utf8String。
    static ca::Result<void, ca::str::Utf8String> write_file(
        const ca::str::Utf8StringRef& path,
        const TomlDocument& document,
        const TomlWriterOptions& options = TomlWriterOptions());
};

}  // namespace ca::toml
