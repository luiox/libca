#pragma once

#include "libca/core/bytes.hpp"

namespace ca::crypto {

/// @brief 常量时间字节串相等比较。
/// @param lhs 左侧字节视图。
/// @param rhs 右侧字节视图。
/// @return 长度一致且内容一致时返回 true。
bool constant_time_eq(ca::core::ByteSlice lhs, ca::core::ByteSlice rhs) noexcept;

/// @brief 抗优化清零（volatile 逐字节写）。
/// @param data 目标缓冲区。
/// @param size 字节数。
/// @note 供密钥材料离开作用域前清零：普通 memset 可能被编译器当作死存储消除，
///       密钥残留栈帧可被栈复用或进程转储读出。size 为 0 时无操作。
void secure_zero(void* data, ca::usize size) noexcept;

}   // namespace ca::crypto
