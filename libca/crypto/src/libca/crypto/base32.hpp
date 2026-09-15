#pragma once

#include "crypto_error.hpp"

#include "libca/core/bytes.hpp"
#include "libca/core/result.hpp"

#include <string>

namespace ca::crypto {

/// @brief 使用 RFC 4648 standard alphabet（A-Z、2-7）进行 Base32 编码。
/// @param data 输入字节视图。
/// @param padding 为 true 时补 '=' 到 8 的倍数长度（默认），为 false 时省略 padding。
/// @return Base32 编码字符串。
std::string base32_encode(ca::core::ByteSlice data, bool padding = true);

/// @brief 严格 Base32 解码。
/// @param src Base32 字符串。只接受大写字母 A-Z 与数字 2-7，小写字母视为非法字符；
///            padding 可省略；带 padding 时只允许出现在末尾且数量必须与数据长度 canonical 匹配。
/// @return 成功返回解码后的字节；字符、长度、padding 或补零位（非 canonical 编码）非法时返回 INVALID_BASE32。
ca::Result<ca::core::Bytes, CryptoError> base32_decode(const std::string& src);

}  // namespace ca::crypto
