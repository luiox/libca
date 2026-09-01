#pragma once

/// @file source_location.hpp
/// @brief TOML 解析过程中的源码位置。
/// @details `SourceLocation` 同时记录字节偏移与 1-based 的行/列，方便错误定位。
///          列按字节计数（UTF-8 多字节字符算多列），文档与实现一致即可。

#include "libca/core/datatype.hpp"

namespace ca::toml {

/// @brief 源码位置：字节偏移 + 行 + 列，均从 1 起计（offset 从 0 起）。
struct SourceLocation
{
    /// 字节偏移（从 0 起）。
    usize offset = 0;
    /// 行号（1-based）。
    usize line = 1;
    /// 列号（1-based，按字节计；UTF-8 多字节字符占多列）。
    usize column = 1;
};

}   // namespace ca::toml
