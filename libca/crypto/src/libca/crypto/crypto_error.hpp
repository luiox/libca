#pragma once

namespace ca::crypto {

/// @brief crypto 模块通用错误码。
enum class CryptoError
{
    INVALID_ARGUMENT,
    INVALID_HEX,
    INVALID_BASE64,
    RANDOM_FAILED,
};

/// @brief 将 CryptoError 转为稳定的调试字符串。
/// @param error crypto 错误码。
/// @return 错误描述字符串。
inline const char* to_string(CryptoError error) noexcept
{
    switch (error) {
    case CryptoError::INVALID_ARGUMENT: return "invalid argument";
    case CryptoError::INVALID_HEX: return "invalid hex input";
    case CryptoError::INVALID_BASE64: return "invalid base64 input";
    case CryptoError::RANDOM_FAILED: return "secure random generation failed";
    }
    return "unknown crypto error";
}

}   // namespace ca::crypto
