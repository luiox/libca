#pragma once

#include <string>
#include <string_view>

/// @file uuid.hpp
/// @brief UUID v4 生成与校验。命名空间 `ca::uuid`。
/// @note 底层随机源复用 `ca::crypto::secure_random_bytes`（系统 CSPRNG），不做伪随机。
///       仅提供随机 v4、空 UUID 与格式校验，不做 v1/v3/v5。

namespace ca::uuid {

/// @brief 生成随机 UUID v4（格式 `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`）。
/// @return 36 字符的小写十六进制 UUID 字符串。
/// @throws std::runtime_error 系统随机源失败时抛出（属致命错误）。
std::string v4();

/// @brief 生成空 UUID（`00000000-0000-0000-0000-000000000000`）。
std::string nil();

/// @brief 校验字符串是否为合法 UUID。
///
/// 接受大小写十六进制、36 字符（含 4 个连字符）。当 `check_variant_version` 为 true
/// 时，额外校验第 3 段首位为 4（v4）且第 4 段首位为 8/9/a/b（variant 1），即严格匹配
/// `v4()` 的输出。
bool is_valid(std::string_view s, bool check_variant_version = true) noexcept;

}   // namespace ca::uuid
