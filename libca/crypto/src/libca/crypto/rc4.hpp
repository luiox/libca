#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

namespace ca::crypto {

/// @brief RC4 stream cipher，供 legacy 协议与混淆场景使用。
/// @param key RC4 key，长度为 1..256 字节。
/// @param data 输入明文或密文。
/// @return 成功返回 XOR 后的数据；key 长度非法返回 INVALID_ARGUMENT。
/// @note 不要用于新的安全敏感加密设计。
ca::Result<ca::core::Bytes, CryptoError> rc4_crypt(ca::core::ByteSlice key,
                                                   ca::core::ByteSlice data);

}  // namespace ca::crypto
