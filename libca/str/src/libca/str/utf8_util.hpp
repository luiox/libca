//
// @brief UTF-8 编解码基础工具函数
// @author Canrad
// @date 2026/05/31
// @note 仅依赖 datatype.hpp，不依赖其他字符串类型
//

#pragma once

#include "libca/core/datatype.hpp"

namespace ca::str {

// 根据 UTF-8 首字节返回码点字节数（1~4），非法返回 0
usize utf8_code_point_bytes(u8 first_byte) noexcept;

// 安全的码点字节数：至少返回 1，避免非法字节导致死循环
inline usize utf8_code_point_bytes_safe(u8 first_byte) noexcept {
    auto n = utf8_code_point_bytes(first_byte);
    return n > 0 ? n : 1;
}

// 从 UTF-8 序列解码出一个码点
u32 utf8_decode_code_point(const u8* bytes) noexcept;

// 将码点编码为 UTF-8 序列写入 out，返回字节数
usize utf8_encode_code_point(u32 cp, u8* out) noexcept;

// 统计 UTF-8 序列中的码点个数，遇非法返回 0 并输出 invalid_pos
usize utf8_count_code_points(const u8* data, usize byte_length,
                             usize* invalid_pos = nullptr) noexcept;

// 检查是否为合法 UTF-8
bool utf8_is_valid(const u8* data, usize byte_length) noexcept;

}  // namespace ca::str
