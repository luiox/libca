#pragma once

#include "libca/core/bytes.hpp"

#include <string>

namespace ca::crypto {

/// @brief 计算 HMAC-SHA1（RFC 2104/2202）。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 20 字节 HMAC-SHA1 digest。
/// @note SHA-1 已不抗碰撞，HMAC-SHA1 仍安全但新场景建议 HMAC-SHA256 及以上。
ca::core::Bytes hmac_sha1(ca::core::ByteSlice key, ca::core::ByteSlice data);

/// @brief 计算 HMAC-SHA1 并返回小写十六进制字符串。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 40 字符小写十六进制 digest。
std::string hmac_sha1_hex(ca::core::ByteSlice key, ca::core::ByteSlice data);

/// @brief 计算 HMAC-SHA256（RFC 2104/4231）。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 32 字节 HMAC-SHA256 digest。
ca::core::Bytes hmac_sha256(ca::core::ByteSlice key, ca::core::ByteSlice data);

/// @brief 计算 HMAC-SHA256 并返回小写十六进制字符串。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 64 字符小写十六进制 digest。
std::string hmac_sha256_hex(ca::core::ByteSlice key, ca::core::ByteSlice data);

/// @brief 计算 HMAC-SHA512（RFC 2104/4231）。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 64 字节 HMAC-SHA512 digest。
ca::core::Bytes hmac_sha512(ca::core::ByteSlice key, ca::core::ByteSlice data);

/// @brief 计算 HMAC-SHA512 并返回小写十六进制字符串。
/// @param key HMAC key。
/// @param data 输入数据。
/// @return 128 字符小写十六进制 digest。
std::string hmac_sha512_hex(ca::core::ByteSlice key, ca::core::ByteSlice data);

}  // namespace ca::crypto
