/// @file utf8_util.hpp
/// @brief UTF-8 编解码基础工具函数。仅依赖 datatype.hpp，不依赖其他字符串类型。
/// @author Canrad
/// @date 2026/05/31

#pragma once

#include "libca/core/datatype.hpp"

namespace ca::str {

/// @brief 根据 UTF-8 首字节返回码点字节数（1~4）；非法首字节返回 0。
usize utf8_code_point_bytes(u8 first_byte) noexcept;

/// @brief 安全版码点字节数：至少返回 1，避免非法字节导致解码死循环。
inline usize utf8_code_point_bytes_safe(u8 first_byte) noexcept
{
    auto n = utf8_code_point_bytes(first_byte);
    return n > 0 ? n : 1;
}

/// @brief 从 UTF-8 序列解码出一个码点。
u32 utf8_decode_code_point(const u8* bytes) noexcept;

/// @brief 校验首字节之后的 clen-1 个续字节是否均形如 10xxxxxx。
/// @note utf8_code_point_bytes 只看首字节、utf8_decode_code_point 不校验续位，
///       直接组合使用会把「合法首字节 + 非法续字节」误解码成错误码点；
///       逐序列解码的调用方须先经本函数确认。
bool utf8_valid_continuation(const u8* bytes, usize clen) noexcept;

/// @brief 将码点编码为 UTF-8 写入 out，返回写入字节数。
usize utf8_encode_code_point(u32 cp, u8* out) noexcept;

/// @brief 统计 UTF-8 序列码点数；遇非法返回 0 并通过 invalid_pos 输出非法位置。
usize utf8_count_code_points(const u8* data, usize byte_length,
                             usize* invalid_pos = nullptr) noexcept;

/// @brief 检查是否为合法 UTF-8。
bool utf8_is_valid(const u8* data, usize byte_length) noexcept;

}   // namespace ca::str
