#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

namespace ca::crypto {

/// @warning DES 仅 56 位有效密钥，已被现代密码学视为不安全：本模块仅用于
///          legacy 数据兼容与混淆场景，勿用于新安全设计。
constexpr ca::usize DES_BLOCK_SIZE = 8;
constexpr ca::usize DES_KEY_SIZE = 8;

/// @brief 计算单个 DES 块加密（FIPS 46-3，NoPadding）。
/// @param key DES_KEY_SIZE（8）字节密钥，奇偶校验位不校验、不调整。
/// @param block DES_BLOCK_SIZE（8）字节明文块。
/// @return 成功返回 8 字节密文块；密钥或块长度非法返回 INVALID_ARGUMENT。
/// @note 纯内置实现，不提供 OpenSSL/CNG 后端（现代提供方已弃用 DES）；
///       弱密钥/半弱密钥不做校验，与 JCE DESKeySpec 语义一致。
ca::Result<ca::core::Bytes, CryptoError> des_encrypt_block(ca::core::ByteSlice key,
                                                           ca::core::ByteSlice block);

/// @brief 计算单个 DES 块解密（FIPS 46-3，NoPadding）。
/// @param key DES_KEY_SIZE（8）字节密钥，奇偶校验位不校验、不调整。
/// @param block DES_BLOCK_SIZE（8）字节密文块。
/// @return 成功返回 8 字节明文块；密钥或块长度非法返回 INVALID_ARGUMENT。
/// @note 纯内置实现，不提供 OpenSSL/CNG 后端；弱密钥/半弱密钥不做校验。
ca::Result<ca::core::Bytes, CryptoError> des_decrypt_block(ca::core::ByteSlice key,
                                                           ca::core::ByteSlice block);

/// @brief 计算 DES-ECB 加密（NoPadding，无链接，相同明文块得到相同密文块）。
/// @param key DES_KEY_SIZE（8）字节密钥。
/// @param plaintext 明文，长度必须为 DES_BLOCK_SIZE 整数倍（可为 0）。
/// @return 成功返回与输入等长的密文；密钥长度或块对齐非法返回 INVALID_ARGUMENT。
/// @note ECB 不隐藏明文模式，长数据建议改用 CBC；填充由调用方用 pkcs5_pad 完成。
ca::Result<ca::core::Bytes, CryptoError> des_ecb_encrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice plaintext);

/// @brief 计算 DES-ECB 解密（NoPadding，去填充由调用方用 pkcs5_unpad 完成）。
/// @param key DES_KEY_SIZE（8）字节密钥。
/// @param ciphertext 密文，长度必须为 DES_BLOCK_SIZE 整数倍（可为 0）。
/// @return 成功返回与输入等长的明文；密钥长度或块对齐非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> des_ecb_decrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice ciphertext);

/// @brief 计算 DES-CBC 加密（NoPadding，PKCS#5 填充由调用方负责）。
/// @param key DES_KEY_SIZE（8）字节密钥。
/// @param iv DES_BLOCK_SIZE（8）字节初始向量；全零 IV 由调用方显式传入。
/// @param plaintext 明文，长度必须为 DES_BLOCK_SIZE 整数倍（可为 0）。
/// @return 成功返回与输入等长的密文；密钥/IV 长度或块对齐非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> des_cbc_encrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice iv,
                                                         ca::core::ByteSlice plaintext);

/// @brief 计算 DES-CBC 解密（NoPadding，填充校验由调用方用 pkcs5_unpad 完成）。
/// @param key DES_KEY_SIZE（8）字节密钥。
/// @param iv DES_BLOCK_SIZE（8）字节初始向量；全零 IV 由调用方显式传入。
/// @param ciphertext 密文，长度必须为 DES_BLOCK_SIZE 整数倍（可为 0）。
/// @return 成功返回与输入等长的明文；密钥/IV 长度或块对齐非法返回 INVALID_ARGUMENT。
ca::Result<ca::core::Bytes, CryptoError> des_cbc_decrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice iv,
                                                         ca::core::ByteSlice ciphertext);

/// @brief 按 PKCS#5 追加填充（DES 块大小 8：pad 值 1..8，整块/空输入补满一块）。
/// @param plaintext 任意长度明文（可为 0）。
/// @return 成功返回填充后数据，长度为 8 的整数倍且至少加 1 字节。
ca::Result<ca::core::Bytes, CryptoError> pkcs5_pad(ca::core::ByteSlice plaintext);

/// @brief 校验并剥离 PKCS#5 填充（对齐 JCE BadPaddingException 语义）。
/// @param padded 填充后数据，长度必须为 DES_BLOCK_SIZE 整数倍且非空。
/// @return 成功返回剥离填充后的数据；填充值越界（0 或 > 8）、不一致或长度非法
///         返回 INVALID_ARGUMENT。
/// @note 填充校验仅为协议正确性，不具备认证能力（可被填充预言攻击利用），
///       安全敏感场景应改用认证加密模式。
ca::Result<ca::core::Bytes, CryptoError> pkcs5_unpad(ca::core::ByteSlice padded);

}  // namespace ca::crypto
