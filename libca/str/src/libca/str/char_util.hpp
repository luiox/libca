//
// @brief 字符类型 (Utf8Char / Utf16Char) 及字符分类工具
// @author Canrad
// @date 2026/05/31
// @note 命名空间 ca::str，基于解码后的码点做字符分类
//       避免在 UTF-8 字节上直接使用 std::isalnum 等
//

#pragma once

#include "libca/core/datatype.hpp"

#include <cwchar>
#include <cwctype>

namespace ca::str {

// ============================================================================
// Utf8Char — 一个解码后的 Unicode 码点（UTF-8 已解码）
// ============================================================================
//
// 存储 u32 码点值，提供字符分类方法（基于码点而非原始字节）。
//
// 使用示例:
//   Utf8Char cp = Utf8Char::fromRaw("\xE4\xB8\xAD");  // '中'
//   if (cp.isAlpha()) { ... }
//   u8 buf[4]; usize n = cp.encode(buf);
//

class Utf8Char {
public:
    // 默认构造：空码点 (U+0000)
    constexpr Utf8Char() noexcept : cp_(0) {}

    // 从码点值构造
    constexpr explicit Utf8Char(u32 codePoint) noexcept : cp_(codePoint) {}

    // 从 UTF-8 字节序列解码构造
    // bytes 必须指向合法的 UTF-8 序列首字节
    static Utf8Char fromRaw(const u8* bytes) noexcept;

    // 从 C 字符串的当前位置解码（相当于 fromRaw 的 char* 重载）
    static Utf8Char fromRaw(const char* bytes) noexcept;

    // 将码点编码为 UTF-8 字节序列写入 out
    // out 必须有至少 4 字节的空间
    // 返回写入的字节数 (1~4)
    usize encode(u8* out) const noexcept;

    // ---- 访问 ----

    constexpr u32 codePoint() const noexcept { return cp_; }
    constexpr explicit operator u32() const noexcept { return cp_; }

    // ---- 字符分类 ----
    // 以下函数基于解码后的码点值判断，可在 UTF-8 场景安全使用

    // 是否为字母 (包含 CJK 等非拉丁文字)
    bool isAlpha() const noexcept;

    // 是否为十进制数字 (0-9)
    bool isDigit() const noexcept;

    // 是否为字母或数字
    bool isAlnum() const noexcept;

    // 是否为空白字符 (含 ASCII 空格、制表符以及 Unicode 空白)
    bool isSpace() const noexcept;

    // 是否为小写字母
    bool isLower() const noexcept;

    // 是否为大写字母
    bool isUpper() const noexcept;

    // 是否为标点符号
    bool isPunct() const noexcept;

    // 是否为可打印字符（含空格）
    bool isPrint() const noexcept;

    // 是否为控制字符
    bool isCntrl() const noexcept;

    // 是否为十六进制数字 (0-9, A-F, a-f)
    bool isXDigit() const noexcept;

    // ---- 大小写转换 ----

    // 转小写（仅 ASCII 和部分拉丁字母可准确转换）
    Utf8Char toLower() const noexcept;

    // 转大写（仅 ASCII 和部分拉丁字母可准确转换）
    Utf8Char toUpper() const noexcept;

    // ---- 比较 ----

    constexpr bool operator==(const Utf8Char& other) const noexcept { return cp_ == other.cp_; }
    constexpr bool operator!=(const Utf8Char& other) const noexcept { return cp_ != other.cp_; }

private:
    u32 cp_;
};

static_assert(sizeof(Utf8Char) == 4, "Utf8Char must be 4 bytes");


// ============================================================================
// Utf16Char — 一个 UTF-16 码元（16 位）
// ============================================================================
//
// 存储 u16 值，可以是 BMP 字符、高位代理或低位代理。
// 提供代理对判定方法。
//
// 使用示例:
//   Utf16Char cu(0xD83D);          // 高位代理
//   if (cu.isLeadSurrogate()) { ... }
//   Utf16Char trail(0xDE00);
//   u32 cp = Utf16Char::decodePair(cu, trail);  // → U+1F600
//

class Utf16Char {
public:
    constexpr Utf16Char() noexcept : unit_(0) {}
    constexpr explicit Utf16Char(u16 unit) noexcept : unit_(unit) {}

    constexpr u16 unit() const noexcept { return unit_; }
    constexpr explicit operator u16() const noexcept { return unit_; }

    // ---- 代理对判定 ----

    // 是否为代理项 (U+D800~U+DFFF)
    constexpr bool isSurrogate() const noexcept;

    // 是否为高位代理 (U+D800~U+DBFF)
    constexpr bool isLeadSurrogate() const noexcept;

    // 是否为低位代理 (U+DC00~U+DFFF)
    constexpr bool isTrailSurrogate() const noexcept;

    // 是否为 BMP 字符 (U+0000~U+D7FF 或 U+E000~U+FFFF)
    constexpr bool isBmp() const noexcept;

    // ---- 解码 / 编码 ----

    // 将一对代理解码为码点
    // high: 高位代理 (U+D800~U+DBFF)
    // low:  低位代理 (U+DC00~U+DFFF)
    static u32 decodePair(Utf16Char high, Utf16Char low) noexcept;

    // 将码点编码为一对代理（若 cp > U+FFFF）
    // 返回是否成功（cp 在 U+10000~U+10FFFF 范围内）
    // high/low 分别输出高低代理
    static bool encodePair(u32 cp, Utf16Char& high, Utf16Char& low) noexcept;

    constexpr bool operator==(const Utf16Char& other) const noexcept { return unit_ == other.unit_; }
    constexpr bool operator!=(const Utf16Char& other) const noexcept { return unit_ != other.unit_; }

private:
    u16 unit_;
};


// ============================================================================
// 字符分类便捷函数（操作解码后的码点值）
// ============================================================================

inline bool isAlpha(u32 cp) noexcept { return Utf8Char(cp).isAlpha(); }
inline bool isDigit(u32 cp) noexcept { return Utf8Char(cp).isDigit(); }
inline bool isAlnum(u32 cp) noexcept { return Utf8Char(cp).isAlnum(); }
inline bool isSpace(u32 cp) noexcept { return Utf8Char(cp).isSpace(); }
inline bool isLower(u32 cp) noexcept { return Utf8Char(cp).isLower(); }
inline bool isUpper(u32 cp) noexcept { return Utf8Char(cp).isUpper(); }
inline bool isPunct(u32 cp) noexcept { return Utf8Char(cp).isPunct(); }
inline bool isPrint(u32 cp) noexcept { return Utf8Char(cp).isPrint(); }
inline bool isCntrl(u32 cp) noexcept { return Utf8Char(cp).isCntrl(); }
inline bool isXDigit(u32 cp) noexcept { return Utf8Char(cp).isXDigit(); }

inline u32 toLower(u32 cp) noexcept { return Utf8Char(cp).toLower().codePoint(); }
inline u32 toUpper(u32 cp) noexcept { return Utf8Char(cp).toUpper().codePoint(); }


// ============================================================================
// Utf16Char 内联实现
// ============================================================================

constexpr bool Utf16Char::isSurrogate() const noexcept {
    return unit_ >= 0xD800 && unit_ <= 0xDFFF;
}

constexpr bool Utf16Char::isLeadSurrogate() const noexcept {
    return unit_ >= 0xD800 && unit_ <= 0xDBFF;
}

constexpr bool Utf16Char::isTrailSurrogate() const noexcept {
    return unit_ >= 0xDC00 && unit_ <= 0xDFFF;
}

constexpr bool Utf16Char::isBmp() const noexcept {
    return unit_ <= 0xD7FF || (unit_ >= 0xE000 && unit_ <= 0xFFFF);
}

inline u32 Utf16Char::decodePair(Utf16Char high, Utf16Char low) noexcept {
    return 0x10000 + ((high.unit_ - 0xD800) << 10) + (low.unit_ - 0xDC00);
}

inline bool Utf16Char::encodePair(u32 cp, Utf16Char& high, Utf16Char& low) noexcept {
    if (cp < 0x10000 || cp > 0x10FFFF)
        return false;
    cp -= 0x10000;
    high = Utf16Char(static_cast<u16>(0xD800 + (cp >> 10)));
    low  = Utf16Char(static_cast<u16>(0xDC00 + (cp & 0x03FF)));
    return true;
}

}  // namespace ca::str
