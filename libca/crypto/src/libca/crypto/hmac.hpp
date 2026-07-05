#pragma once

#include "libca/core/bytes.hpp"

#include <string>

namespace ca::crypto {

/// @brief 计算 HMAC-SHA256。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 32 字节 HMAC-SHA256 digest。
ca::core::Bytes hmac_sha256(ca::core::ByteSlice key, ca::core::ByteSlice data);

/// @brief 计算 HMAC-SHA256 并返回小写十六进制字符串。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 64 字符小写十六进制 digest。
std::string hmac_sha256_hex(ca::core::ByteSlice key, ca::core::ByteSlice data);

}  // namespace ca::crypto
