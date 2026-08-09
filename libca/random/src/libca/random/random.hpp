#pragma once

#include <string>
#include <string_view>

#include "libca/core/datatype.hpp"

/// @file random.hpp
/// @brief 高层随机数接口。命名空间 `ca::random`。
/// @note 底层随机源复用 `ca::crypto::secure_random_bytes`（系统 CSPRNG），不实现伪
///       随机数引擎、不提供可复现种子。所有函数线程安全（无共享可变状态）。

namespace ca::random {

/// @brief 用系统 CSPRNG 填充缓冲区。
/// @throws std::runtime_error 系统随机源失败时抛出（属致命错误）。
void fill_bytes(void* buf, usize len);

/// @brief 生成 [0, n) 范围内的均匀随机非负整数。
/// @throws std::runtime_error 系统随机源失败时抛出。
/// @throws std::invalid_argument n == 0 时抛出（Release 构建下断言被剥离，异常兜底）。
/// @note 使用拒绝采样消除模偏差。
u64 next(u64 n);

/// @brief 生成 [lo, hi) 范围内的均匀随机整数。
/// @throws std::runtime_error 系统随机源失败时抛出。
/// @throws std::invalid_argument lo >= hi 时抛出（Release 构建下断言被剥离，异常兜底）。
u64 range(u64 lo, u64 hi);

/// @brief 生成 [0.0, 1.0) 范围内的均匀随机双精度浮点数。
/// @throws std::runtime_error 系统随机源失败时抛出。
double probability();

/// @brief 生成指定字节长度的随机十六进制字符串。
/// @param len 字节数；返回字符串长度为 2 * len。
/// @return 小写十六进制字符串。
/// @throws std::runtime_error 系统随机源失败时抛出。
std::string hex_string(usize len);

/// @brief 生成指定长度的随机字母数字字符串（[0-9a-zA-Z]，62 个字符）。
/// @param len 字符串长度。
/// @throws std::runtime_error 系统随机源失败时抛出。
std::string alphanumeric_string(usize len);

}  // namespace ca::random
