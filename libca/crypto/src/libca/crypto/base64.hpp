///
/// @brief Base64 编解码
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::crypto，提供标准 Base64 编码/解码
///

#pragma once

#include <string>
#include <vector>

namespace ca::crypto {

/// Base64 编码
/// @param src 输入数据
/// @param len 数据长度
/// @return Base64 编码字符串
std::string base64Encode(const char* src, size_t len);

/// Base64 编码（std::string 重载）
inline std::string base64Encode(const std::string& src) {
    return base64Encode(src.data(), src.size());
}

/// Base64 解码
/// @param src Base64 编码字符串
/// @return 解码后的字节数组
std::vector<char> base64Decode(const std::string& src);

} // namespace ca::crypto
