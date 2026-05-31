//
// @brief 字符类型实现 (Utf8Char 分类方法)
// @author Canrad
// @date 2026/05/31
// @note
//   ASCII 范围 (0x00-0x7F) 使用标准 ctype 函数。
//   非 ASCII 范围委托给 iswalnum/iswalpha 等宽字符分类函数。
//   大小写转换同理（受当前 locale 影响）。
//   如需完整的 Unicode 大小写折叠，推荐使用 ICU 库。
//

#include <libca/str/char_util.hpp>

#include <cctype>
#include <cwchar>
#include <cwctype>

namespace ca::str {

// ============================================================================
// Utf8Char
// ============================================================================

Utf8Char Utf8Char::fromRaw(const u8* bytes) noexcept {
    if (bytes == nullptr) return Utf8Char(0);
    auto b0 = bytes[0];
    if ((b0 & 0x80) == 0)
        return Utf8Char(b0);
    if ((b0 & 0xE0) == 0xC0) {
        // 需要至少 2 字节，检查后续字节有效性
        if (bytes[1] == '\0') return Utf8Char(0);
        return Utf8Char(((u32)(b0 & 0x1F) << 6) | ((u32)(bytes[1] & 0x3F)));
    }
    if ((b0 & 0xF0) == 0xE0) {
        if (bytes[1] == '\0' || bytes[2] == '\0') return Utf8Char(0);
        return Utf8Char(((u32)(b0 & 0x0F) << 12) |
                        ((u32)(bytes[1] & 0x3F) << 6) |
                        ((u32)(bytes[2] & 0x3F)));
    }
    if ((b0 & 0xF8) == 0xF0) {
        if (bytes[1] == '\0' || bytes[2] == '\0' || bytes[3] == '\0') return Utf8Char(0);
        return Utf8Char(((u32)(b0 & 0x07) << 18) |
                        ((u32)(bytes[1] & 0x3F) << 12) |
                        ((u32)(bytes[2] & 0x3F) << 6) |
                        ((u32)(bytes[3] & 0x3F)));
    }
    return Utf8Char(0);  // 非法
}

Utf8Char Utf8Char::fromRaw(const char* bytes) noexcept {
    return fromRaw(reinterpret_cast<const u8*>(bytes));
}

usize Utf8Char::encode(u8* out) const noexcept {
    if (cp_ <= 0x007F) {
        out[0] = static_cast<u8>(cp_);
        return 1;
    }
    if (cp_ <= 0x07FF) {
        out[0] = static_cast<u8>(0xC0 | (cp_ >> 6));
        out[1] = static_cast<u8>(0x80 | (cp_ & 0x3F));
        return 2;
    }
    if (cp_ <= 0xFFFF) {
        out[0] = static_cast<u8>(0xE0 | (cp_ >> 12));
        out[1] = static_cast<u8>(0x80 | ((cp_ >> 6) & 0x3F));
        out[2] = static_cast<u8>(0x80 | (cp_ & 0x3F));
        return 3;
    }
    // 0x10000 ~ 0x10FFFF
    out[0] = static_cast<u8>(0xF0 | (cp_ >> 18));
    out[1] = static_cast<u8>(0x80 | ((cp_ >> 12) & 0x3F));
    out[2] = static_cast<u8>(0x80 | ((cp_ >> 6) & 0x3F));
    out[3] = static_cast<u8>(0x80 | (cp_ & 0x3F));
    return 4;
}

static bool isCjk(u32 cp) noexcept {
    // CJK 统一表意文字
    return (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||   // CJK 扩展 A
           (cp >= 0x20000 && cp <= 0x2A6DF) ||  // CJK 扩展 B
           (cp >= 0x2A700 && cp <= 0x2B73F) ||  // CJK 扩展 C
           (cp >= 0x2B740 && cp <= 0x2B81F) ||  // CJK 扩展 D
           (cp >= 0x2B820 && cp <= 0x2CEAF) ||  // CJK 扩展 E
           (cp >= 0x3000 && cp <= 0x303F) ||     // CJK 符号和标点
           (cp >= 0xFF00 && cp <= 0xFFEF);       // 半角/全角兼容
}

static bool isHangul(u32 cp) noexcept {
    return (cp >= 0xAC00 && cp <= 0xD7AF) ||  // 谚文音节
           (cp >= 0x1100 && cp <= 0x11FF) ||  // 谚文字母
           (cp >= 0x3130 && cp <= 0x318F);     // 谚文兼容字母
}

static bool isKatakanaHiragana(u32 cp) noexcept {
    return (cp >= 0x3040 && cp <= 0x309F) ||  // 平假名
           (cp >= 0x30A0 && cp <= 0x30FF) ||  // 片假名
           (cp >= 0xFF65 && cp <= 0xFF9F);     // 半角片假名
}

// 判断码点是否为字母（含非拉丁文字）
bool Utf8Char::isAlpha() const noexcept {
    if (cp_ <= 0x7F)
        return std::isalpha(static_cast<int>(cp_)) != 0;
    if (isCjk(cp_) || isHangul(cp_) || isKatakanaHiragana(cp_))
        return true;
    return iswalpha(static_cast<wint_t>(cp_)) != 0;
}

bool Utf8Char::isDigit() const noexcept {
    if (cp_ <= 0x7F)
        return std::isdigit(static_cast<int>(cp_)) != 0;
    // 全角数字 ０-９ (U+FF10-U+FF19)
    if (cp_ >= 0xFF10 && cp_ <= 0xFF19)
        return true;
    return iswdigit(static_cast<wint_t>(cp_)) != 0;
}

bool Utf8Char::isAlnum() const noexcept {
    return isAlpha() || isDigit();
}

bool Utf8Char::isSpace() const noexcept {
    if (cp_ <= 0x7F)
        return std::isspace(static_cast<int>(cp_)) != 0;
    // Unicode 空白字符
    if (cp_ == 0x00A0 || cp_ == 0x1680 ||
        cp_ == 0x2000 || cp_ == 0x2001 || cp_ == 0x2002 ||
        cp_ == 0x2003 || cp_ == 0x2004 || cp_ == 0x2005 ||
        cp_ == 0x2006 || cp_ == 0x2007 || cp_ == 0x2008 ||
        cp_ == 0x2009 || cp_ == 0x200A || cp_ == 0x2028 ||
        cp_ == 0x2029 || cp_ == 0x202F || cp_ == 0x205F ||
        cp_ == 0x3000 || cp_ == 0xFEFF)
        return true;
    return iswspace(static_cast<wint_t>(cp_)) != 0;
}

bool Utf8Char::isLower() const noexcept {
    if (cp_ <= 0x7F)
        return std::islower(static_cast<int>(cp_)) != 0;
    return iswlower(static_cast<wint_t>(cp_)) != 0;
}

bool Utf8Char::isUpper() const noexcept {
    if (cp_ <= 0x7F)
        return std::isupper(static_cast<int>(cp_)) != 0;
    return iswupper(static_cast<wint_t>(cp_)) != 0;
}

bool Utf8Char::isPunct() const noexcept {
    if (cp_ <= 0x7F)
        return std::ispunct(static_cast<int>(cp_)) != 0;
    if (isCjk(cp_) || isHangul(cp_) || isKatakanaHiragana(cp_))
        return false;  // 这些是文字，不是标点
    return iswpunct(static_cast<wint_t>(cp_)) != 0;
}

bool Utf8Char::isPrint() const noexcept {
    if (cp_ <= 0x7F)
        return std::isprint(static_cast<int>(cp_)) != 0;
    if (cp_ <= 0x9F)  // C1 控制字符
        return false;
    return true;  // 非 ASCII 且非控制 → 可打印
}

bool Utf8Char::isCntrl() const noexcept {
    if (cp_ <= 0x7F)
        return std::iscntrl(static_cast<int>(cp_)) != 0;
    // C1 控制字符 (0x80~0x9F)
    if (cp_ >= 0x80 && cp_ <= 0x9F)
        return true;
    return iswcntrl(static_cast<wint_t>(cp_)) != 0;
}

bool Utf8Char::isXDigit() const noexcept {
    if (cp_ <= 0x7F)
        return std::isxdigit(static_cast<int>(cp_)) != 0;
    // 全角十六进制数字 A-F, a-f
    if ((cp_ >= 0xFF21 && cp_ <= 0xFF26) ||  // 全角 A-F
        (cp_ >= 0xFF41 && cp_ <= 0xFF46))     // 全角 a-f
        return true;
    return iswxdigit(static_cast<wint_t>(cp_)) != 0;
}

Utf8Char Utf8Char::toLower() const noexcept {
    if (cp_ <= 0x7F)
        return Utf8Char(static_cast<u32>(std::tolower(static_cast<int>(cp_))));
    auto result = towlower(static_cast<wint_t>(cp_));
    if (result == static_cast<wint_t>(cp_))
        return *this;  // 无变化
    return Utf8Char(static_cast<u32>(result));
}

Utf8Char Utf8Char::toUpper() const noexcept {
    if (cp_ <= 0x7F)
        return Utf8Char(static_cast<u32>(std::toupper(static_cast<int>(cp_))));
    auto result = towupper(static_cast<wint_t>(cp_));
    if (result == static_cast<wint_t>(cp_))
        return *this;
    return Utf8Char(static_cast<u32>(result));
}

}  // namespace ca::str
