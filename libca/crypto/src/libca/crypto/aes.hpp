#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"

namespace ca::crypto {

constexpr ca::usize AES_BLOCK_SIZE = 16;
constexpr ca::usize AES_GCM_NONCE_SIZE = 12;
constexpr ca::usize AES_GCM_TAG_SIZE = 16;

/// @brief AES 运算后端选择。
/// @note Builtin 为纯软件参考实现（正确性与无外部依赖优先，不承诺常量时间）；
///       防御时序攻击的场景应显式选择 OpenSSL/CNG 后端。
enum class AesBackend
{
    Builtin,  ///< 内置参考实现，任何构建配置都可用
    OpenSsl,  ///< OpenSSL EVP 接口，仅 when with_openssl=y 编译期可用
    Cng,      ///< Windows CNG（bcrypt.dll），仅 Windows 平台可用
    Auto,     ///< OpenSSL（编译期可用时）> CNG（Windows）> Builtin
};

/// @brief 查询某后端在当前编译配置/平台下是否可用。
/// @param backend 目标后端；Auto 折算为其解析结果。
/// @return 可用返回 true；显式指定但编译期不可用返回 false。
bool aes_backend_available(AesBackend backend) noexcept;

/// @brief 计算原始 AES-ECB 加密（无填充，输入必须是 16 字节整块）。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param plaintext 明文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @param backend 后端选择，默认 Auto。编译期不可用的后端返回 UNSUPPORTED_ALGORITHM。
/// @return 成功返回与输入等长的密文；密钥长度或输入长度非法返回 INVALID_ARGUMENT。
/// @note Builtin 为参考实现，不承诺常量时间；防御时序攻击场景选 OpenSsl/Cng。
ca::Result<ca::core::Bytes, CryptoError> aes_ecb_encrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice plaintext,
                                                         AesBackend backend = AesBackend::Auto);

/// @brief 计算原始 AES-ECB 解密（无填充，输入必须是 16 字节整块）。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param ciphertext 密文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @param backend 后端选择，默认 Auto。编译期不可用的后端返回 UNSUPPORTED_ALGORITHM。
/// @return 成功返回与输入等长的明文；密钥长度或输入长度非法返回 INVALID_ARGUMENT。
/// @note Builtin 为参考实现，不承诺常量时间；防御时序攻击场景选 OpenSsl/Cng。
ca::Result<ca::core::Bytes, CryptoError> aes_ecb_decrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice ciphertext,
                                                         AesBackend backend = AesBackend::Auto);

/// @brief 计算原始 AES-CBC 加密（PKCS#7 由调用方负责，本函数不填充）。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param iv 16 字节初始向量。
/// @param plaintext 明文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @param backend 后端选择，默认 Auto。编译期不可用的后端返回 UNSUPPORTED_ALGORITHM。
/// @return 成功返回与输入等长的密文；密钥/IV/输入长度非法返回 INVALID_ARGUMENT。
/// @note Builtin 为参考实现，不承诺常量时间；防御时序攻击场景选 OpenSsl/Cng。
ca::Result<ca::core::Bytes, CryptoError> aes_cbc_encrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice iv,
                                                         ca::core::ByteSlice plaintext,
                                                         AesBackend backend = AesBackend::Auto);

/// @brief 计算原始 AES-CBC 解密（去填充由调用方负责，本函数不校验填充）。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param iv 16 字节初始向量。
/// @param ciphertext 密文，长度必须为 AES_BLOCK_SIZE 整数倍。
/// @param backend 后端选择，默认 Auto。编译期不可用的后端返回 UNSUPPORTED_ALGORITHM。
/// @return 成功返回与输入等长的明文；密钥/IV/输入长度非法返回 INVALID_ARGUMENT。
/// @note Builtin 为参考实现，不承诺常量时间；防御时序攻击场景选 OpenSsl/Cng。
ca::Result<ca::core::Bytes, CryptoError> aes_cbc_decrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice iv,
                                                         ca::core::ByteSlice ciphertext,
                                                         AesBackend backend = AesBackend::Auto);

/// @brief AES-CTR 流式加密/解密（加解密同函数，keystream 与数据 XOR）。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param counter_block 16 字节初始计数值，按 128 位大端整数递增（SP800-38A）。
/// @param data 任意长度输入。
/// @param backend 后端选择，默认 Auto。编译期不可用的后端返回 UNSUPPORTED_ALGORITHM。
/// @return 成功返回与输入等长的输出；密钥或计数块长度非法返回 INVALID_ARGUMENT。
/// @note Builtin 为参考实现，不承诺常量时间；防御时序攻击场景选 OpenSsl/Cng。
ca::Result<ca::core::Bytes, CryptoError> aes_ctr_crypt(ca::core::ByteSlice key,
                                                       ca::core::ByteSlice counter_block,
                                                       ca::core::ByteSlice data,
                                                       AesBackend backend = AesBackend::Auto);

/// @brief AES-GCM 加密结果：密文与认证标签分离返回。
struct AesGcmResult
{
    ca::core::Bytes ciphertext;  ///< 与明文等长的密文
    ca::core::Bytes tag;         ///< AES_GCM_TAG_SIZE 字节认证标签
};

/// @brief AES-GCM 认证加密（仅 OpenSsl/Cng 外部后端路径；内置不做 GCM）。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param nonce AES_GCM_NONCE_SIZE（12）字节 nonce。
/// @param plaintext 任意长度明文。
/// @param aad 附加认证数据（不加密但参与认证），可为空。
/// @param backend 后端选择，默认 Auto。Builtin 路径返回 UNSUPPORTED_ALGORITHM。
/// @return 成功返回密文与 16 字节 tag；参数非法返回 INVALID_ARGUMENT；
///         后端不可用返回 UNSUPPORTED_ALGORITHM；后端调用失败返回 BACKEND_FAILED。
ca::Result<AesGcmResult, CryptoError> aes_gcm_encrypt(ca::core::ByteSlice key,
                                                      ca::core::ByteSlice nonce,
                                                      ca::core::ByteSlice plaintext,
                                                      ca::core::ByteSlice aad,
                                                      AesBackend backend = AesBackend::Auto);

/// @brief AES-GCM 认证解密（仅 OpenSsl/Cng 外部后端路径；内置不做 GCM）。
/// @param key 16/24/32 字节密钥（AES-128/192/256）。
/// @param nonce AES_GCM_NONCE_SIZE（12）字节 nonce。
/// @param ciphertext 密文。
/// @param tag AES_GCM_TAG_SIZE（16）字节认证标签。
/// @param aad 附加认证数据，须与加密侧一致，可为空。
/// @param backend 后端选择，默认 Auto。Builtin 路径返回 UNSUPPORTED_ALGORITHM。
/// @return 成功返回明文；tag 或 AAD 不匹配返回 AUTHENTICATION_FAILED；
///         参数非法返回 INVALID_ARGUMENT；后端不可用返回 UNSUPPORTED_ALGORITHM。
ca::Result<ca::core::Bytes, CryptoError> aes_gcm_decrypt(ca::core::ByteSlice key,
                                                         ca::core::ByteSlice nonce,
                                                         ca::core::ByteSlice ciphertext,
                                                         ca::core::ByteSlice tag,
                                                         ca::core::ByteSlice aad,
                                                         AesBackend backend = AesBackend::Auto);

}  // namespace ca::crypto
