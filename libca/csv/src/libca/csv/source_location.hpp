#pragma once

/// @file source_location.hpp
/// @brief CSV 解析过程中的源码位置。
/// @details 与 libca/json、libca/ini 的 SourceLocation 同形态；本模块暂不依赖它们，
///          等多格式库稳定后再考虑回抽公共层。

#include "libca/core/datatype.hpp"

namespace ca::csv {

/// @brief 源码位置：1-based 行 + 列（按字节计；UTF-8 多字节字符占多列）。
struct SourceLocation {
    /// 行号（1-based）。
    usize line = 1;
    /// 列号（1-based，按字节计）。
    usize column = 1;
};

}  // namespace ca::csv
