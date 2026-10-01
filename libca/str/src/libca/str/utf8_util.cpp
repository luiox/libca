#include "utf8_util.hpp"

namespace ca::str {
// ============================================================================
// UTF-8 编解码工具函数
// ============================================================================

usize utf8_code_point_bytes(u8 first_byte) noexcept {
    if ((first_byte & 0x80) == 0)
        return 1;   // 0xxxxxxx
    if ((first_byte & 0xE0) == 0xC0)
        return 2;   // 110xxxxx
    if ((first_byte & 0xF0) == 0xE0)
        return 3;   // 1110xxxx
    if ((first_byte & 0xF8) == 0xF0)
        return 4;   // 11110xxx
    return 0;       // 非法首字节
}

u32 utf8_decode_code_point(const u8* bytes) noexcept {
    auto b0 = bytes[0];
    if ((b0 & 0x80) == 0) {
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0) {
        return ((u32)(b0 & 0x1F) << 6) |
               ((u32)(bytes[1] & 0x3F));
    }
    if ((b0 & 0xF0) == 0xE0) {
        return ((u32)(b0 & 0x0F) << 12) |
               ((u32)(bytes[1] & 0x3F) << 6) |
               ((u32)(bytes[2] & 0x3F));
    }
    if ((b0 & 0xF8) == 0xF0) {
        return ((u32)(b0 & 0x07) << 18) |
               ((u32)(bytes[1] & 0x3F) << 12) |
               ((u32)(bytes[2] & 0x3F) << 6) |
               ((u32)(bytes[3] & 0x3F));
    }
    return 0;  // 非法
}

bool utf8_valid_continuation(const u8* bytes, usize clen) noexcept {
    for (usize i = 1; i < clen; ++i) {
        if ((bytes[i] & 0xC0) != 0x80)
            return false;
    }
    return true;
}

namespace {

// 解出 len 个字节的码点值；调用方须已保证 len 与首字节形状匹配、续字节合法。
u32 utf8_sequence_value(const u8* data, usize len) noexcept {
    switch (len) {
        case 1:  return data[0];
        case 2:  return (u32(data[0] & 0x1F) << 6) | u32(data[1] & 0x3F);
        case 3:  return (u32(data[0] & 0x0F) << 12) | (u32(data[1] & 0x3F) << 6) | u32(data[2] & 0x3F);
        default: return (u32(data[0] & 0x07) << 18) | (u32(data[1] & 0x3F) << 12) |
                        (u32(data[2] & 0x3F) << 6) | u32(data[3] & 0x3F);
    }
}

// Unicode 标准合法性（W3C/UTF-8 要求拒绝）：overlong 表达、UTF-16 代理区
// （U+D800~U+DFFF）与超出 U+10FFFF 的码点均不合法。
bool utf8_sequence_valid(u32 cp, usize len) noexcept {
    if (len == 2 && cp < 0x80) return false;                       // overlong，如 C0 80
    if (len == 3) {
        if (cp < 0x800) return false;                              // overlong，如 E0 80 80
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;            // 代理项，如 ED A0 80
    }
    if (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) return false; // 超范围，如 F4 90 80 80
    return true;
}

}  // namespace

usize utf8_encode_code_point(u32 cp, u8* out) noexcept {
    // 排除非法码点：代理项 (U+D800~U+DFFF) 和超出范围的值
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return 0;

    if (cp <= 0x007F) {
        out[0] = static_cast<u8>(cp);
        return 1;
    }
    if (cp <= 0x07FF) {
        out[0] = static_cast<u8>(0xC0 | (cp >> 6));
        out[1] = static_cast<u8>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        out[0] = static_cast<u8>(0xE0 | (cp >> 12));
        out[1] = static_cast<u8>(0x80 | ((cp >> 6) & 0x3F));
        out[2] = static_cast<u8>(0x80 | (cp & 0x3F));
        return 3;
    }
    // 0x10000 ~ 0x10FFFF
    out[0] = static_cast<u8>(0xF0 | (cp >> 18));
    out[1] = static_cast<u8>(0x80 | ((cp >> 12) & 0x3F));
    out[2] = static_cast<u8>(0x80 | ((cp >> 6) & 0x3F));
    out[3] = static_cast<u8>(0x80 | (cp & 0x3F));
    return 4;
}

usize utf8_count_code_points(const u8* data, usize byte_length,
                             usize* invalid_pos) noexcept {
    usize count = 0;
    usize pos   = 0;

    while (pos < byte_length) {
        auto len = utf8_code_point_bytes(data[pos]);
        if (len == 0 || pos + len > byte_length) {
            // 遇到非法序列
            if (invalid_pos)
                *invalid_pos = pos;
            return 0;
        }
        // 检查后续字节是否都是 10xxxxxx
        for (usize i = 1; i < len; ++i) {
            if ((data[pos + i] & 0xC0) != 0x80) {
                if (invalid_pos)
                    *invalid_pos = pos + i;
                return 0;
            }
        }
        // 码点值级合法性：overlong / 代理区 / 超出 U+10FFFF
        if (!utf8_sequence_valid(utf8_sequence_value(data + pos, len), len)) {
            if (invalid_pos)
                *invalid_pos = pos;
            return 0;
        }
        pos += len;
        ++count;
    }

    return count;
}

bool utf8_is_valid(const u8* data, usize byte_length) noexcept {
    usize pos = 0;
    while (pos < byte_length) {
        auto len = utf8_code_point_bytes(data[pos]);
        if (len == 0 || pos + len > byte_length)
            return false;
        for (usize i = 1; i < len; ++i) {
            if ((data[pos + i] & 0xC0) != 0x80)
                return false;
        }
        if (!utf8_sequence_valid(utf8_sequence_value(data + pos, len), len))
            return false;
        pos += len;
    }
    return true;
}



}
