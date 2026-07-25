/// @file char_util.hpp
/// @brief 字符类型 Utf8Char（解码后的 Unicode 码点）/ Utf16Char（UTF-16 码元）及字符分类工具。
/// @author Canrad
/// @date 2026/05/31
/// @note 分类基于**解码后的码点**，不要在 UTF-8 原始字节上直接用 std::isalnum 等。
///       大小写转换仅 ASCII 和部分拉丁字母准确，不做完整 Unicode 折叠。

#pragma once

#include "libca/core/datatype.hpp"

#include <cwchar>
#include <cwctype>

namespace ca::str {

/// @brief 一个解码后的 Unicode 码点（4 字节），提供基于码点的字符分类。
/// @code
///   Utf8Char cp = Utf8Char::from_raw("\xE4\xB8\xAD");  // '中'
///   if (cp.is_alpha()) { u8 buf[4]; usize n = cp.encode(buf); }
/// @endcode
class Utf8Char {
public:
    // 默认构造：空码点 (U+0000)
    constexpr Utf8Char() noexcept : cp_(0) {}

    // 从码点值构造
    constexpr explicit Utf8Char(u32 code_point) noexcept : cp_(code_point) {}

    // 从 UTF-8 字节序列解码构造
    // bytes 必须指向合法的 UTF-8 序列首字节
    static Utf8Char from_raw(const u8* bytes) noexcept;

    // 从 C 字符串的当前位置解码（相当于 from_raw 的 char* 重载）
    static Utf8Char from_raw(const char* bytes) noexcept;

    // 将码点编码为 UTF-8 字节序列写入 out
    // out 必须有至少 4 字节的空间
    // 返回写入的字节数 (1~4)
    usize encode(u8* out) const noexcept;

    // ---- 访问 ----

    constexpr u32 code_point() const noexcept { return cp_; }
    constexpr explicit operator u32() const noexcept { return cp_; }

    // ---- 字符分类 ----
    // 以下函数基于解码后的码点值判断，可在 UTF-8 场景安全使用

    // 是否为字母 (包含 CJK 等非拉丁文字)
    bool is_alpha() const noexcept;

    // 是否为十进制数字 (0-9)
    bool is_digit() const noexcept;

    // 是否为字母或数字
    bool is_alnum() const noexcept;

    // 是否为空白字符 (含 ASCII 空格、制表符以及 Unicode 空白)
    bool is_space() const noexcept;

    // 是否为小写字母
    bool is_lower() const noexcept;

    // 是否为大写字母
    bool is_upper() const noexcept;

    // 是否为标点符号
    bool is_punct() const noexcept;

    // 是否为可打印字符（含空格）
    bool is_print() const noexcept;

    // 是否为控制字符
    bool is_cntrl() const noexcept;

    // 是否为十六进制数字 (0-9, A-F, a-f)
    bool is_xdigit() const noexcept;

    // ---- 大小写转换 ----

    // 转小写（仅 ASCII 和部分拉丁字母可准确转换）
    Utf8Char to_lower() const noexcept;

    // 转大写（仅 ASCII 和部分拉丁字母可准确转换）
    Utf8Char to_upper() const noexcept;

    // ---- 比较 ----

    constexpr bool operator==(const Utf8Char& other) const noexcept { return cp_ == other.cp_; }
    constexpr bool operator!=(const Utf8Char& other) const noexcept { return cp_ != other.cp_; }

private:
    u32 cp_;
};

static_assert(sizeof(Utf8Char) == 4, "Utf8Char must be 4 bytes");


/// @brief 一个 UTF-16 码元（16 位）：可为 BMP 字符、高位或低位代理。提供代理对判定/编解码。
/// @code
///   Utf16Char cu(0xD83D);  // 高位代理
///   if (cu.is_lead_surrogate()) { u32 cp = Utf16Char::decode_pair(cu, Utf16Char(0xDE00)); }
/// @endcode
class Utf16Char {
public:
    constexpr Utf16Char() noexcept : unit_(0) {}
    constexpr explicit Utf16Char(u16 unit) noexcept : unit_(unit) {}

    constexpr u16 unit() const noexcept { return unit_; }
    constexpr explicit operator u16() const noexcept { return unit_; }

    // ---- 代理对判定 ----

    // 是否为代理项 (U+D800~U+DFFF)
    constexpr bool is_surrogate() const noexcept;

    // 是否为高位代理 (U+D800~U+DBFF)
    constexpr bool is_lead_surrogate() const noexcept;

    // 是否为低位代理 (U+DC00~U+DFFF)
    constexpr bool is_trail_surrogate() const noexcept;

    // 是否为 BMP 字符 (U+0000~U+D7FF 或 U+E000~U+FFFF)
    constexpr bool is_bmp() const noexcept;

    // ---- 解码 / 编码 ----

    // 将一对代理解码为码点
    // high: 高位代理 (U+D800~U+DBFF)
    // low:  低位代理 (U+DC00~U+DFFF)
    static u32 decode_pair(Utf16Char high, Utf16Char low) noexcept;

    // 将码点编码为一对代理（若 cp > U+FFFF）
    // 返回是否成功（cp 在 U+10000~U+10FFFF 范围内）
    // high/low 分别输出高低代理
    static bool encode_pair(u32 cp, Utf16Char& high, Utf16Char& low) noexcept;

    constexpr bool operator==(const Utf16Char& other) const noexcept { return unit_ == other.unit_; }
    constexpr bool operator!=(const Utf16Char& other) const noexcept { return unit_ != other.unit_; }

private:
    u16 unit_;
};


// ============================================================================
// 字符分类便捷函数（操作解码后的码点值）
// ============================================================================

inline bool is_alpha(u32 cp) noexcept { return Utf8Char(cp).is_alpha(); }
inline bool is_digit(u32 cp) noexcept { return Utf8Char(cp).is_digit(); }
inline bool is_alnum(u32 cp) noexcept { return Utf8Char(cp).is_alnum(); }
inline bool is_space(u32 cp) noexcept { return Utf8Char(cp).is_space(); }
inline bool is_lower(u32 cp) noexcept { return Utf8Char(cp).is_lower(); }
inline bool is_upper(u32 cp) noexcept { return Utf8Char(cp).is_upper(); }
inline bool is_punct(u32 cp) noexcept { return Utf8Char(cp).is_punct(); }
inline bool is_print(u32 cp) noexcept { return Utf8Char(cp).is_print(); }
inline bool is_cntrl(u32 cp) noexcept { return Utf8Char(cp).is_cntrl(); }
inline bool is_xdigit(u32 cp) noexcept { return Utf8Char(cp).is_xdigit(); }

inline u32 to_lower(u32 cp) noexcept { return Utf8Char(cp).to_lower().code_point(); }
inline u32 to_upper(u32 cp) noexcept { return Utf8Char(cp).to_upper().code_point(); }


// ============================================================================
// Utf16Char 内联实现
// ============================================================================

constexpr bool Utf16Char::is_surrogate() const noexcept {
    return unit_ >= 0xD800 && unit_ <= 0xDFFF;
}

constexpr bool Utf16Char::is_lead_surrogate() const noexcept {
    return unit_ >= 0xD800 && unit_ <= 0xDBFF;
}

constexpr bool Utf16Char::is_trail_surrogate() const noexcept {
    return unit_ >= 0xDC00 && unit_ <= 0xDFFF;
}

constexpr bool Utf16Char::is_bmp() const noexcept {
    return unit_ <= 0xD7FF || (unit_ >= 0xE000 && unit_ <= 0xFFFF);
}

inline u32 Utf16Char::decode_pair(Utf16Char high, Utf16Char low) noexcept {
    return 0x10000 + ((high.unit_ - 0xD800) << 10) + (low.unit_ - 0xDC00);
}

inline bool Utf16Char::encode_pair(u32 cp, Utf16Char& high, Utf16Char& low) noexcept {
    if (cp < 0x10000 || cp > 0x10FFFF)
        return false;
    cp -= 0x10000;
    high = Utf16Char(static_cast<u16>(0xD800 + (cp >> 10)));
    low  = Utf16Char(static_cast<u16>(0xDC00 + (cp & 0x03FF)));
    return true;
}

}  // namespace ca::str
