//
// @brief 字符串类型间转换函数实现
// @author Canrad
// @date 2026/05/31
//

#include <libca/str/conversion.hpp>
#include <libca/str/char_util.hpp>

#include <cstring>
#include <memory>
#include <stdexcept>

namespace ca::str {

// ============================================================================
// Utf8String ↔ CString
// ============================================================================

CString toCString(const Utf8StringRef& str) {
    // Utf8String 内部是 u8[] = UTF-8 编码 → 直接当 char[] 拷贝
    return CString(reinterpret_cast<const char*>(str.data()), str.byteLength());
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
    auto  len  = str.byteLength();

    // 第一遍：计算需要的 wchar_t 数量
    // 对于 UTF-32 (Linux/macOS): 每个码点 = 1 wchar_t
    // 对于 UTF-16 (Windows): 码点 > U+FFFF 需要 2 个 wchar_t (代理对)
    usize pos = 0;
    usize wcCount = 0;
    while (pos < len) {
        auto clen = utf8CodePointBytes(data[pos]);
        if (clen == 0 || pos + clen > len) {
            throw std::runtime_error("toWString: invalid UTF-8 sequence");
        }
        auto cp = utf8DecodeCodePoint(data + pos);
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
        auto clen = utf8CodePointBytes(data[pos]);
        auto cp   = utf8DecodeCodePoint(data + pos);
#if defined(_WIN32)
        if (cp >= 0x10000 && cp <= 0x10FFFF) {
            Utf16Char high, low;
            Utf16Char::encodePair(cp, high, low);
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
        if (Utf16Char(wc).isLeadSurrogate()) {
            if (i + 1 >= len) {
                throw std::runtime_error("toUtf8String: truncated surrogate pair");
            }
            auto low = static_cast<u16>(data[i + 1]);
            if (!Utf16Char(low).isTrailSurrogate()) {
                throw std::runtime_error("toUtf8String: invalid surrogate pair");
            }
            cp = Utf16Char::decodePair(Utf16Char(wc), Utf16Char(low));
            ++i;  // 消耗低位代理
        } else if (Utf16Char(wc).isTrailSurrogate()) {
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
        if (Utf16Char(wc).isLeadSurrogate()) {
            auto low = static_cast<u16>(data[i + 1]);
            cp = Utf16Char::decodePair(Utf16Char(wc), Utf16Char(low));
            ++i;
        } else if (Utf16Char(wc).isTrailSurrogate()) {
            continue;  // 不应到达
        } else {
            cp = wc;
        }
#else
        cp = static_cast<u32>(data[i]);
#endif
        outPos += utf8EncodeCodePoint(cp, buf.get() + outPos);
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
        auto clen = utf8CodePointBytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return 0;
        auto cp = utf8DecodeCodePoint(utf8 + pos);
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
        auto clen = utf8CodePointBytes(utf8[pos]);
        if (clen == 0 || pos + clen > byteLength)
            return 0;
        auto cp = utf8DecodeCodePoint(utf8 + pos);
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return 0;
        if (cp >= 0x10000) {
            Utf16Char high, low;
            Utf16Char::encodePair(cp, high, low);
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
        if (Utf16Char(utf16[i]).isLeadSurrogate()) {
            if (i + 1 >= unitCount || !Utf16Char(utf16[i + 1]).isTrailSurrogate())
                return 0;
            cp = Utf16Char::decodePair(Utf16Char(utf16[i]), Utf16Char(utf16[i + 1]));
            ++i;
        } else if (Utf16Char(utf16[i]).isTrailSurrogate()) {
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
        if (Utf16Char(utf16[i]).isLeadSurrogate()) {
            cp = Utf16Char::decodePair(Utf16Char(utf16[i]), Utf16Char(utf16[i + 1]));
            ++i;
        } else if (Utf16Char(utf16[i]).isTrailSurrogate()) {
            return 0;
        } else {
            cp = utf16[i];
        }
        out += utf8EncodeCodePoint(cp, utf8 + out);
    }
    return out;
}

}  // namespace ca::str
