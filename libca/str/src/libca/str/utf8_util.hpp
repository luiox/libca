//
// @brief UTF-8 编解码基础工具函数
// @author Canrad
// @date 2026/05/31
// @note 仅依赖 datatype.hpp，不依赖其他字符串类型
//

#ifndef LIBCA_STR_UTF8_UTIL_HPP
#define LIBCA_STR_UTF8_UTIL_HPP

#include "libca/core/datatype.hpp"

namespace ca::str {

// 根据 UTF-8 首字节返回码点字节数（1~4），非法返回 0
usize utf8CodePointBytes(u8 firstByte) noexcept;

// 安全的码点字节数：至少返回 1，避免非法字节导致死循环
inline usize utf8CodePointBytesSafe(u8 firstByte) noexcept {
    auto n = utf8CodePointBytes(firstByte);
    return n > 0 ? n : 1;
}

// 从 UTF-8 序列解码出一个码点
u32 utf8DecodeCodePoint(const u8* bytes) noexcept;

// 将码点编码为 UTF-8 序列写入 out，返回字节数
usize utf8EncodeCodePoint(u32 cp, u8* out) noexcept;

// 统计 UTF-8 序列中的码点个数，遇非法返回 0 并输出 invalidPos
usize utf8CountCodePoints(const u8* data, usize byteLength,
                          usize* invalidPos = nullptr) noexcept;

// 检查是否为合法 UTF-8
bool utf8IsValid(const u8* data, usize byteLength) noexcept;

}  // namespace ca::str

#endif  // LIBCA_STR_UTF8_UTIL_HPP
