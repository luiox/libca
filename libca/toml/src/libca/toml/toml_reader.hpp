#pragma once

/// @file toml_reader.hpp
/// @brief TOML DOM 入口：TomlReader。把字符串/文件解析为 TomlDocument。
/// @details 内部用 TomlParser 直接构造 DOM（TOML 表组织需全局视图，SAX 无意义，
///          因此本模块不做 SAX/DOM 分离，与 ini 一致）。

#include "libca/core/result.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/toml/parse_error.hpp"
#include "libca/toml/source_location.hpp"
#include "libca/toml/toml_document.hpp"

namespace ca::toml {

/// @brief TOML 解析选项（TOML 1.0 严格模式，目前无可配项，保留扩展位）。
struct TomlReaderOptions
{
    // TOML 1.0 没什么可配的；保留扩展位。
};

/// @brief TOML DOM 读取器。
class TomlReader
{
public:
    /// @brief 从字符串解析 TOML 为 TomlDocument。
    /// @param input TOML 文本视图（须在使用期内有效）。
    /// @param options 解析选项。
    /// @return 成功返回 TomlDocument；失败返回 ParseError。
    static ca::Result<TomlDocument, ParseError> read(
        const ca::str::Utf8StringRef& input,
        const TomlReaderOptions&      options = TomlReaderOptions());

    /// @brief 从文件解析 TOML 为 TomlDocument。
    /// @param path 文件路径。
    /// @param options 解析选项。
    /// @return 成功返回 TomlDocument；打开失败或格式错误返回 ParseError。
    static ca::Result<TomlDocument, ParseError> read_file(
        const ca::str::Utf8StringRef& path, const TomlReaderOptions& options = TomlReaderOptions());
};

}   // namespace ca::toml
