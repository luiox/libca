#pragma once

/// @file parse_error.hpp
/// @brief TOML 解析错误：位置 + 人读消息。
/// @details 不用异常，错误经 `ca::Result<T, ParseError>` 传播。`message` 为 Utf8String，
///          可直接拼接到上层报告。

#include "libca/toml/source_location.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::toml {

/// @brief 解析错误：发生位置 + 描述。
struct ParseError {
    /// 错误发生位置。
    SourceLocation location;
    /// 人读的错误描述。
    ca::str::Utf8String message;
};

}  // namespace ca::toml
