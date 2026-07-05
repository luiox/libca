#pragma once

#include "libca/core/bytes.hpp"

namespace ca::crypto {

/// @brief 常量时间字节串相等比较。
/// @param lhs 左侧字节视图。
/// @param rhs 右侧字节视图。
/// @return 长度一致且内容一致时返回 true。
bool constant_time_eq(ca::core::ByteSlice lhs, ca::core::ByteSlice rhs) noexcept;

}  // namespace ca::crypto
