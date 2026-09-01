#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

#include <string>

namespace ca::crypto {

/// @brief 将字节视图编码为小写十六进制字符串。
/// @param data 输入字节视图。
/// @return 小写十六进制字符串。
std::string hex_encode(ca::core::ByteSlice data);

/// @brief 将十六进制字符串解码为字节。
/// @param text 十六进制字符串，必须为偶数长度。
/// @return 成功返回字节；长度或字符非法时返回 INVALID_HEX。
ca::Result<ca::core::Bytes, CryptoError> hex_decode(const std::string& text);

/// @brief 将原始内存编码为小写十六进制字符串。
/// @param data 输入内存指针。
/// @param len 输入字节数。
/// @return 小写十六进制字符串。
inline std::string hex_encode(const void* data, ca::usize len)
{
    return hex_encode(ca::core::ByteSlice(static_cast<const ca::u8*>(data), len));
}

/// @brief 将 std::string 的原始字节编码为小写十六进制字符串。
/// @param text 输入字符串。
/// @return 小写十六进制字符串。
inline std::string hex_encode(const std::string& text)
{
    return hex_encode(text.data(), text.size());
}

}   // namespace ca::crypto
