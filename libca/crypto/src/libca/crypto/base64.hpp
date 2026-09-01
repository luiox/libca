///
/// @brief Base64 编解码
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::crypto，提供标准 Base64 编码/解码
///

#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

#include <string>

namespace ca::crypto {

/// @brief 使用 libca 字节视图进行 Base64 编码。
/// @param data 输入字节视图。
/// @return Base64 编码字符串。
std::string base64_encode(ca::core::ByteSlice data);

/// @brief 严格 Base64 解码。
/// @param src Base64 字符串。
/// @return 成功返回解码后的字节；格式、padding 或补零位非法时返回 INVALID_BASE64。
ca::Result<ca::core::Bytes, CryptoError> base64_decode(const std::string& src);

}   // namespace ca::crypto
