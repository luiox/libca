#pragma once

/// @file parse_error.hpp
/// @brief INI 解析错误：位置 + 人读消息。

#include "libca/ini/source_location.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::ini {

/// @brief 解析错误：发生位置 + 描述。
struct ParseError {
    /// 错误发生位置。
    SourceLocation location;
    /// 人读的错误描述。
    ca::str::Utf8String message;
};

}  // namespace ca::ini
