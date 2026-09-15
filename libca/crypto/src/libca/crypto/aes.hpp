#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"

namespace ca::crypto {

constexpr ca::usize AES_BLOCK_SIZE = 16;

/// @brief 计算原始 AES-ECB 加密（无填充，输入必须是 16 字节整块）。
/// @note 参考实现：正确性与无外部依赖优先，未做常量时间加固；防御时序攻击
///       场景应选择 OpenSSL/CNG 后端版本。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param plaintext 明文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @return 成功返回与输入等长的密文；密钥长度或输入长度非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> aes_ecb_encrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice plaintext);

/// @brief 计算原始 AES-ECB 解密（无填充，输入必须是 16 字节整块）。
/// @note 参考实现：正确性与无外部依赖优先，未做常量时间加固；防御时序攻击
///       场景应选择 OpenSSL/CNG 后端版本。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param ciphertext 密文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @return 成功返回与输入等长的明文；密钥长度或输入长度非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> aes_ecb_decrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice ciphertext);

/// @brief 计算原始 AES-CBC 加密（PKCS#7 由调用方负责，本函数不填充）。
/// @note 参考实现：正确性与无外部依赖优先，未做常量时间加固；防御时序攻击
///       场景应选择 OpenSSL/CNG 后端版本。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param iv 16 字节初始向量。
/// @param plaintext 明文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @return 成功返回与输入等长的密文；密钥/IV/输入长度非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> aes_cbc_encrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice iv,
                                                         ca::core::ByteSlice plaintext);

/// @brief 计算原始 AES-CBC 解密（去填充由调用方负责，本函数不校验填充）。
/// @note 参考实现：正确性与无外部依赖优先，未做常量时间加固；防御时序攻击
///       场景应选择 OpenSSL/CNG 后端版本。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param iv 16 字节初始向量。
/// @param ciphertext 密文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @return 成功返回与输入等长的明文；密钥/IV/输入长度非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> aes_cbc_decrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice iv,
                                                         ca::core::ByteSlice ciphertext);

/// @brief AES-CTR 流式加密/解密（加解密同函数，keystream 与数据 XOR）。
/// @note 参考实现：正确性与无外部依赖优先，未做常量时间加固；防御时序攻击
///       场景应选择 OpenSSL/CNG 后端版本。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param counter_block 16 字节初始计数值，按 128 位大端整数递增（SP800-38A）。
/// @param data 任意长度输入。
/// @return 成功返回与输入等长的输出；密钥或计数块长度非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> aes_ctr_crypt(ca::core::ByteSlice key,
                                                       ca::core::ByteSlice counter_block,
                                                       ca::core::ByteSlice data);

}  // namespace ca::crypto
