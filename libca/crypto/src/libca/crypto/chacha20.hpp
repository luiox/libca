#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

namespace ca::crypto {

constexpr ca::usize CHACHA20_KEY_SIZE   = 32;
constexpr ca::usize CHACHA20_NONCE_SIZE = 12;
constexpr ca::usize CHACHA20_BLOCK_SIZE = 64;

/// @brief RFC 8439 ChaCha20 block function.
/// @param key 32 字节密钥。
/// @param counter 32 位 block counter。
/// @param nonce 12 字节 nonce。
/// @return 成功返回 64 字节 keystream block；参数长度错误返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> chacha20_block(ca::core::ByteSlice key, ca::u32 counter,
                                                        ca::core::ByteSlice nonce);

/// @brief 使用 RFC 8439 ChaCha20 keystream 对数据进行 XOR。
/// @param key 32 字节密钥。
/// @param counter 初始 block counter。
/// @param nonce 12 字节 nonce。
/// @param data 输入明文或密文。
/// @return 成功返回 XOR 后的数据；参数长度错误返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> chacha20_xor(ca::core::ByteSlice key, ca::u32 counter,
                                                      ca::core::ByteSlice nonce,
                                                      ca::core::ByteSlice data);

}   // namespace ca::crypto
