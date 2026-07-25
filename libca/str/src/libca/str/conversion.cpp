//
// @brief 字符串类型间转换函数实现
// @author Canrad
// @date 2026/05/31
//

#include "conversion.hpp"
#include "char_util.hpp"
#include "utf8_util.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>

namespace ca::str {

// ============================================================================
// Utf8String ↔ CString
// ============================================================================

CString toCString(const Utf8StringRef& str) {
    // Utf8String 内部是 u8[] = UTF-8 编码 → 直接当 char[] 拷贝
    return CString(reinterpret_cast<const char*>(str.data()), str.byte_length());
}

Utf8String toUtf8String(const CStringRef& str) {
    // C 字符串的 char 数据视为 UTF-8 编码
    return Utf8String(reinterpret_cast<const u8*>(str.data()), str.length());
}


// ============================================================================
// Utf8String ↔ WString
// ============================================================================

WString toWString(const Utf8StringRef& str) {
    auto  data = str.data();
    auto  len  = str.byte_length();

    // 第一遍：计算需要的 wchar_t 数量
    // 对于 UTF-32 (Linux/macOS): 每个码点 = 1 wchar_t
    // 对于 UTF-16 (Windows): 码点 > U+FFFF 需要 2 个 wchar_t (代理对)
    usize pos = 0;
    usize wcCount = 0;
    while (pos < len) {
        auto clen = utf8_code_point_bytes(data[pos]);
        if (clen == 0 || pos + clen > len) {
            throw std::runtime_error("toWString: invalid UTF-8 sequence");
        }
        auto cp = utf8_decode_code_point(data + pos);
#if defined(_WIN32)
        // Windows: wchar_t = UTF-16LE
        if (cp >= 0x10000 && cp <= 0x10FFFF) {
            wcCount += 2;  // 代理对
        } else {
            wcCount += 1;
        }
#else
        // Linux/macOS: wchar_t = UTF-32
        wcCount += 1;
#endif
        pos += clen;
    }

    // 分配并填充
    auto buf = std::unique_ptr<wchar_t[]>(new wchar_t[wcCount + 1]);
    pos = 0;
    usize outIdx = 0;
    while (pos < len) {
        auto clen = utf8_code_point_bytes(data[pos]);
        auto cp   = utf8_decode_code_point(data + pos);
#if defined(_WIN32)
        if (cp >= 0x10000 && cp <= 0x10FFFF) {
            Utf16Char high, low;
            Utf16Char::encode_pair(cp, high, low);
            buf[outIdx++] = static_cast<wchar_t>(high.unit());
            buf[outIdx++] = static_cast<wchar_t>(low.unit());
        } else {
            buf[outIdx++] = static_cast<wchar_t>(cp);
        }
#else
        buf[outIdx++] = static_cast<wchar_t>(cp);
#endif
        pos += clen;
    }
    buf[wcCount] = L'\0';

    return WString(buf.get(), wcCount);
}

Utf8String toUtf8String(const WStringRef& str) {
    auto data = str.data();
    auto len  = str.length();

    // 第一遍：计算 UTF-8 字节数
    usize byteCount = 0;
    for (usize i = 0; i < len; ++i) {
        u32 cp;
#if defined(_WIN32)
        // Windows: wchar_t = UTF-16LE
        auto wc = static_cast<u16>(data[i]);
        if (Utf16Char(wc).is_lead_surrogate()) {
            if (i + 1 >= len) {
                throw std::runtime_error("toUtf8String: truncated surrogate pair");
            }
            auto low = static_cast<u16>(data[i + 1]);
            if (!Utf16Char(low).is_trail_surrogate()) {
                throw std::runtime_error("toUtf8String: invalid surrogate pair");
            }
            cp = Utf16Char::decode_pair(Utf16Char(wc), Utf16Char(low));
            ++i;  // 消耗低位代理
        } else if (Utf16Char(wc).is_trail_surrogate()) {
            throw std::runtime_error("toUtf8String: unexpected trail surrogate");
        } else {
            cp = wc;
        }
#else
        // Linux/macOS: wchar_t = UTF-32
        cp = static_cast<u32>(data[i]);
#endif
        // 检查码点合法性
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            throw std::runtime_error("toUtf8String: invalid code point");
        }
        if (cp <= 0x7F)       byteCount += 1;
        else if (cp <= 0x7FF) byteCount += 2;
        else if (cp <= 0xFFFF) byteCount += 3;
        else                   byteCount += 4;
    }

    // 分配并编码
    auto buf = std::unique_ptr<u8[]>(new u8[byteCount + 1]);
    usize outPos = 0;
    for (usize i = 0; i < len; ++i) {
        u32 cp;
#if defined(_WIN32)
        auto wc = static_cast<u16>(data[i]);
        if (Utf16Char(wc).is_lead_surrogate()) {
            auto low = static_cast<u16>(data[i + 1]);
            cp = Utf16Char::decode_pair(Utf16Char(wc), Utf16Char(low));
            ++i;
        } else if (Utf16Char(wc).is_trail_surrogate()) {
            continue;  // 不应到达
        } else {
            cp = wc;
        }
#else
        cp = static_cast<u32>(data[i]);
#endif
        outPos += utf8_encode_code_point(cp, buf.get() + outPos);
    }
    buf[byteCount] = '\0';

    return Utf8String(buf.get(), byteCount);
}


// ============================================================================
// CString ↔ WString（经 UTF-8 中转）
// ============================================================================

WString toWString(const CStringRef& str) {
    // CString → Utf8String → WString
    auto utf8 = toUtf8String(str);
    return toWString(utf8.ref());
}

CString toCString(const WStringRef& str) {
    // WString → Utf8String → CString
    auto utf8 = toUtf8String(str);
    return toCString(utf8.ref());
}


// ============================================================================
// UTF-8 ↔ raw UTF-16 工具
// ============================================================================

usize utf8ToUtf16Length(const u8* utf8, usize byteLength) noexcept {
    usize pos = 0;
    usize count = 0;
    while (pos < byteLength) {
        auto clen = utf8_code_point_bytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return 0;
        auto cp = utf8_decode_code_point(utf8 + pos);
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return 0;
        count += (cp >= 0x10000) ? 2 : 1;
        pos += clen;
    }
    return count;
}

usize utf8ToUtf16(const u8* utf8, usize byteLength, u16* utf16) noexcept {
    usize pos = 0;
    usize out = 0;
    while (pos < byteLength) {
        auto clen = utf8_code_point_bytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return 0;
        auto cp = utf8_decode_code_point(utf8 + pos);
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return 0;
        if (cp >= 0x10000) {
            Utf16Char high, low;
            Utf16Char::encode_pair(cp, high, low);
            utf16[out++] = high.unit();
            utf16[out++] = low.unit();
        } else {
            utf16[out++] = static_cast<u16>(cp);
        }
        pos += clen;
    }
    return out;
}

usize utf16ToUtf8Length(const u16* utf16, usize unitCount) noexcept {
    usize byteCount = 0;
    for (usize i = 0; i < unitCount; ++i) {
        u32 cp;
        if (Utf16Char(utf16[i]).is_lead_surrogate()) {
            if (i + 1 >= unitCount || !Utf16Char(utf16[i + 1]).is_trail_surrogate())
                return 0;
            cp = Utf16Char::decode_pair(Utf16Char(utf16[i]), Utf16Char(utf16[i + 1]));
            ++i;
        } else if (Utf16Char(utf16[i]).is_trail_surrogate()) {
            return 0;
        } else {
            cp = utf16[i];
        }
        if (cp <= 0x7F)       byteCount += 1;
        else if (cp <= 0x7FF) byteCount += 2;
        else if (cp <= 0xFFFF) byteCount += 3;
        else                   byteCount += 4;
    }
    return byteCount;
}

usize utf16ToUtf8(const u16* utf16, usize unitCount, u8* utf8) noexcept {
    usize out = 0;
    for (usize i = 0; i < unitCount; ++i) {
        u32 cp;
        if (Utf16Char(utf16[i]).is_lead_surrogate()) {
            // 高代理必须后接一个 trail 代理：先查边界再取下一码元，避免末尾孤立
            // 高代理时越界读 utf16[i+1]，并拒绝非 trail 的非法搭配。
            if (i + 1 >= unitCount || !Utf16Char(utf16[i + 1]).is_trail_surrogate())
                return 0;
            cp = Utf16Char::decode_pair(Utf16Char(utf16[i]), Utf16Char(utf16[i + 1]));
            ++i;
        } else if (Utf16Char(utf16[i]).is_trail_surrogate()) {
            return 0;
        } else {
            cp = utf16[i];
        }
        out += utf8_encode_code_point(cp, utf8 + out);
    }
    return out;
}


// ============================================================================
// UTF-8 ↔ UTF-32
// ============================================================================

usize utf8_to_utf32_length(const u8* utf8, usize byteLength) noexcept {
    usize pos = 0;
    usize count = 0;
    while (pos < byteLength) {
        auto clen = utf8_code_point_bytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return 0;
        auto cp = utf8_decode_code_point(utf8 + pos);
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return 0;
        ++count;
        pos += clen;
    }
    return count;
}

usize utf8_to_utf32(const u8* utf8, usize byteLength, u32* utf32) noexcept {
    usize pos = 0;
    usize out = 0;
    while (pos < byteLength) {
        auto clen = utf8_code_point_bytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return 0;
        auto cp = utf8_decode_code_point(utf8 + pos);
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return 0;
        utf32[out++] = cp;
        pos += clen;
    }
    return out;
}

usize utf32_to_utf8_length(const u32* utf32, usize count) noexcept {
    usize byteCount = 0;
    for (usize i = 0; i < count; ++i) {
        u32 cp = utf32[i];
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return 0;
        if (cp <= 0x7F)        byteCount += 1;
        else if (cp <= 0x7FF)  byteCount += 2;
        else if (cp <= 0xFFFF) byteCount += 3;
        else                   byteCount += 4;
    }
    return byteCount;
}

usize utf32_to_utf8(const u32* utf32, usize count, u8* utf8) noexcept {
    usize out = 0;
    for (usize i = 0; i < count; ++i) {
        auto n = utf8_encode_code_point(utf32[i], utf8 + out);
        if (n == 0)  // utf8_encode_code_point 已拒绝代理项 / >U+10FFFF
            return 0;
        out += n;
    }
    return out;
}


// ============================================================================
// Latin-1 (ISO-8859-1) ↔ UTF-8
// ============================================================================

usize latin1_to_utf8_length(const u8* latin1, usize length) noexcept {
    usize byteCount = 0;
    for (usize i = 0; i < length; ++i)
        byteCount += (latin1[i] < 0x80) ? 1 : 2;
    return byteCount;
}

usize latin1_to_utf8(const u8* latin1, usize length, u8* utf8) noexcept {
    usize out = 0;
    for (usize i = 0; i < length; ++i) {
        u8 b = latin1[i];
        if (b < 0x80) {
            utf8[out++] = b;
        } else {
            // U+0080..U+00FF → 2 字节 UTF-8
            utf8[out++] = static_cast<u8>(0xC0 | (b >> 6));
            utf8[out++] = static_cast<u8>(0x80 | (b & 0x3F));
        }
    }
    return out;
}

usize utf8_to_latin1_length(const u8* utf8, usize byteLength) noexcept {
    usize pos = 0;
    usize count = 0;
    while (pos < byteLength) {
        auto clen = utf8_code_point_bytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return UTF8_TO_LATIN1_INVALID;
        auto cp = utf8_decode_code_point(utf8 + pos);
        if (cp > 0x00FF)  // Latin-1 只能表示 U+0000..U+00FF
            return UTF8_TO_LATIN1_INVALID;
        ++count;
        pos += clen;
    }
    return count;
}

usize utf8_to_latin1(const u8* utf8, usize byteLength, u8* latin1) noexcept {
    usize pos = 0;
    usize out = 0;
    while (pos < byteLength) {
        auto clen = utf8_code_point_bytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return UTF8_TO_LATIN1_INVALID;
        auto cp = utf8_decode_code_point(utf8 + pos);
        if (cp > 0x00FF)
            return UTF8_TO_LATIN1_INVALID;
        latin1[out++] = static_cast<u8>(cp);
        pos += clen;
    }
    return out;
}

}  // namespace ca::str
