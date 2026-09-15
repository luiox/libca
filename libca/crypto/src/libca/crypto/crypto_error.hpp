#pragma once

namespace ca::crypto {

/// @brief crypto 模块通用错误码。
enum class CryptoError
{
    INVALID_ARGUMENT,
    INVALID_HEX,
    INVALID_BASE64,
    INVALID_BASE32,
    RANDOM_FAILED,
    // 为后续 OpenSSL/CNG 等后端适配预留：算法不被当前后端支持。
    UNSUPPORTED_ALGORITHM,
};

/// @brief 将 CryptoError 转为稳定的调试字符串。
/// @param error crypto 错误码。
/// @return 错误描述字符串。
inline const char* to_string(CryptoError error) noexcept
{
    switch (error) {
    case CryptoError::INVALID_ARGUMENT:
        return "invalid argument";
    case CryptoError::INVALID_HEX:
        return "invalid hex input";
    case CryptoError::INVALID_BASE64:
        return "invalid base64 input";
    case CryptoError::INVALID_BASE32:
        return "invalid base32 input";
    case CryptoError::RANDOM_FAILED:
        return "secure random generation failed";
    case CryptoError::UNSUPPORTED_ALGORITHM:
        return "unsupported algorithm";
    }
    return "unknown crypto error";
}

}  // namespace ca::crypto
