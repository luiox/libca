#pragma once

#include "libca/core/bytes.hpp"
#include "libca/crypto/crypto_error.hpp"

namespace ca::crypto {

/// @brief PBKDF2-HMAC-SHA256 口令基派生（RFC 2898 5.2，向量出处 RFC 7914 §11）。
/// @param password 口令字节。
/// @param salt 盐值。
/// @param iterations 迭代次数 c，必须 >= 1（RFC 2898 要求正整数）。
/// @param dk_len 派生密钥长度，必须 >= 1 字节。
/// @return dk_len 字节派生密钥。
/// @note 错误：iterations == 0 或 dk_len == 0 时返回
///       Err(CryptoError::INVALID_ARGUMENT)，不回退到默认值。
ca::core::Result<ca::core::Bytes, CryptoError> pbkdf2_hmac_sha256(
    ca::core::ByteSlice password, ca::core::ByteSlice salt, ca::usize iterations,
    ca::usize dk_len);

}  // namespace ca::crypto
