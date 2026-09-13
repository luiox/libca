#pragma once

#include "libca/core/bytes.hpp"
#include "libca/crypto/crypto_error.hpp"

#include <string>

namespace ca::crypto {

// HKDF（RFC 5869）：Extract + Expand 两步 KDF。
// extract/expand 按哈希算法各提供一组；derive 为 extract+expand 的一次性便捷封装。

/// @brief HKDF-Extract（RFC 5869 2.2）：PRK = HMAC-Hash(salt, IKM)。
/// @param salt 盐值；允许为空，此时等价于 HashLen 个 0 的盐（HMAC key 补零语义）。
/// @param ikm 输入密钥材料。
/// @return HashLen 字节的 PRK。
ca::core::Bytes hkdf_sha1_extract(ca::core::ByteSlice salt, ca::core::ByteSlice ikm);

/// @brief HKDF-Expand（RFC 5869 2.3）：由 PRK 扩展任意长度 OKM。
/// @param prk 至少 HashLen 字节的伪随机密钥。
/// @param info 上下文/应用信息，允许为空。
/// @param length 期望输出字节数；0 返回空 Bytes。
/// @return length 字节 OKM。
/// @note 错误：prk 长度不足 HashLen 或 length > 255 * HashLen 时返回
///       Err(CryptoError::INVALID_ARGUMENT)。
ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha1_expand(
    ca::core::ByteSlice prk, ca::core::ByteSlice info, ca::usize length);

/// @brief 一次性 HKDF（SHA-1）：derive = extract + expand。
/// @param salt 盐值，允许为空。
/// @param ikm 输入密钥材料。
/// @param info 上下文信息，允许为空。
/// @param length 期望输出字节数。
/// @return length 字节 OKM；错误条件同 hkdf_sha1_expand。
ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha1_derive(
    ca::core::ByteSlice salt, ca::core::ByteSlice ikm, ca::core::ByteSlice info, ca::usize length);

/// @brief HKDF-Extract（SHA-256 版本），语义同 hkdf_sha1_extract。
ca::core::Bytes hkdf_sha256_extract(ca::core::ByteSlice salt, ca::core::ByteSlice ikm);

/// @brief HKDF-Expand（SHA-256 版本），语义同 hkdf_sha1_expand。
ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha256_expand(
    ca::core::ByteSlice prk, ca::core::ByteSlice info, ca::usize length);

/// @brief 一次性 HKDF（SHA-256 版本），语义同 hkdf_sha1_derive。
ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha256_derive(
    ca::core::ByteSlice salt, ca::core::ByteSlice ikm, ca::core::ByteSlice info, ca::usize length);

/// @brief HKDF-Extract（SHA-512 版本），语义同 hkdf_sha1_extract。
ca::core::Bytes hkdf_sha512_extract(ca::core::ByteSlice salt, ca::core::ByteSlice ikm);

/// @brief HKDF-Expand（SHA-512 版本），语义同 hkdf_sha1_expand。
ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha512_expand(
    ca::core::ByteSlice prk, ca::core::ByteSlice info, ca::usize length);

/// @brief 一次性 HKDF（SHA-512 版本），语义同 hkdf_sha1_derive。
ca::core::Result<ca::core::Bytes, CryptoError> hkdf_sha512_derive(
    ca::core::ByteSlice salt, ca::core::ByteSlice ikm, ca::core::ByteSlice info, ca::usize length);

}  // namespace ca::crypto
