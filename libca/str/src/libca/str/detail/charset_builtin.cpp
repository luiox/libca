//
// @brief 字符编码内置转换核心实现（Tier 1 纯算法）。
// @author Canrad
// @date 2026/09/15
//

#include "libca/str/detail/charset_builtin.hpp"

#include "libca/str/conversion.hpp"
#include "libca/str/detail/charset_internal.hpp"
#include "libca/str/utf8_util.hpp"

namespace ca::str::detail {

namespace {

#if defined(_WIN32)
// Windows：wchar_t 即 UTF-16 码元，直接复用 conversion.hpp 的 UTF-8 ↔ UTF-16 原语。
core::StatusResult<std::wstring> utf8_to_wide_via_utf16(std::string_view utf8)
{
    const usize units = utf8_to_utf16_length(reinterpret_cast<const u8*>(utf8.data()),
                                             utf8.size());
    if (units == 0 && !utf8.empty())
        return core::Err(charset_invalid_sequence("utf-8", 0));

    std::u16string units_buf(units, u16{0});
    if (utf8_to_utf16(reinterpret_cast<const u8*>(utf8.data()), utf8.size(),
                      reinterpret_cast<u16*>(units_buf.data()))
        == 0)
        return core::Err(charset_invalid_sequence("utf-8", 0));

    std::wstring wide;
    wide.reserve(units);
    for (u16 unit : units_buf)
        wide.push_back(static_cast<wchar_t>(unit));
    return core::Ok<std::wstring>(std::move(wide));
}

core::StatusResult<std::string> wide_to_utf8_via_utf16(std::wstring_view wide)
{
    std::u16string units_buf;
    units_buf.reserve(wide.size());
    for (wchar_t ch : wide)
        units_buf.push_back(static_cast<u16>(ch));

    const usize bytes = utf16_to_utf8_length(reinterpret_cast<const u16*>(units_buf.data()),
                                             units_buf.size());
    if (bytes == 0 && !units_buf.empty())
        return core::Err(charset_invalid_sequence("utf-16", 0));

    std::string utf8(bytes, '\0');
    if (utf16_to_utf8(reinterpret_cast<const u16*>(units_buf.data()), units_buf.size(),
                      reinterpret_cast<u8*>(utf8.data()))
        == 0)
        return core::Err(charset_invalid_sequence("utf-16", 0));
    return core::Ok<std::string>(std::move(utf8));
}
#else
// POSIX：wchar_t 为 UCS-4，经码点序列互转（严格标量值校验）。
core::StatusResult<std::wstring> utf8_to_wide_via_ucs4(std::string_view utf8)
{
    const u8*    data = reinterpret_cast<const u8*>(utf8.data());
    const usize  size = utf8.size();

    std::wstring wide;
    wide.reserve(size);
    usize pos = 0;
    while (pos < size) {
        u32 cp = 0;
        if (!utf8_next_code_point(data, size, pos, cp))
            return core::Err(charset_invalid_sequence("utf-8", pos));
        wide_push_code_point(wide, cp);
    }
    return core::Ok<std::wstring>(std::move(wide));
}

core::StatusResult<std::string> wide_to_utf8_via_ucs4(std::wstring_view wide)
{
    auto cps = wide_to_code_points(wide);
    if (cps.is_err())
        return core::Err(cps.unwrap_err());

    // 第二遍编码：合法标量值编码必成功，总字节数 <= 4 * 码点数。
    std::string utf8;
    utf8.reserve(cps.unwrap().size() * 4);
    u8 buf[4];
    for (u32 cp : std::move(cps).unwrap()) {
        const usize n = utf8_encode_code_point(cp, buf);
        utf8.append(reinterpret_cast<const char*>(buf), n);
    }
    return core::Ok<std::string>(std::move(utf8));
}
#endif

// Windows-1252 差异表：byte 0x80-0x9F → 码点（32 项含 5 项 C1 恒等）。
// 27 个非恒等项出处：WHATWG Encoding Standard index-windows-1252
// （https://encoding.spec.whatwg.org/index-windows-1252.txt）。
constexpr u16 CP1252_HIGH_TABLE[32] = {
    0x20AC,  // 0x80 EURO SIGN
    0x0081,  // 0x81 WHATWG 未定义 → Latin-1 恒等（C1 control）
    0x201A,  // 0x82 SINGLE LOW-9 QUOTATION MARK
    0x0192,  // 0x83 LATIN SMALL LETTER F WITH HOOK
    0x201E,  // 0x84 DOUBLE LOW-9 QUOTATION MARK
    0x2026,  // 0x85 HORIZONTAL ELLIPSIS
    0x2020,  // 0x86 DAGGER
    0x2021,  // 0x87 DOUBLE DAGGER
    0x02C6,  // 0x88 MODIFIER LETTER CIRCUMFLEX ACCENT
    0x2030,  // 0x89 PER MILLE SIGN
    0x0160,  // 0x8A LATIN CAPITAL LETTER S WITH CARON
    0x2039,  // 0x8B SINGLE LEFT-POINTING ANGLE QUOTATION MARK
    0x0152,  // 0x8C LATIN CAPITAL LIGATURE OE
    0x008D,  // 0x8D WHATWG 未定义 → Latin-1 恒等（C1 control）
    0x017D,  // 0x8E LATIN CAPITAL LETTER Z WITH CARON
    0x008F,  // 0x8F WHATWG 未定义 → Latin-1 恒等（C1 control）
    0x0090,  // 0x90 WHATWG 未定义 → Latin-1 恒等（C1 control）
    0x2018,  // 0x91 LEFT SINGLE QUOTATION MARK
    0x2019,  // 0x92 RIGHT SINGLE QUOTATION MARK
    0x201C,  // 0x93 LEFT DOUBLE QUOTATION MARK
    0x201D,  // 0x94 RIGHT DOUBLE QUOTATION MARK
    0x2022,  // 0x95 BULLET
    0x2013,  // 0x96 EN DASH
    0x2014,  // 0x97 EM DASH
    0x02DC,  // 0x98 SMALL TILDE
    0x2122,  // 0x99 TRADE MARK SIGN
    0x0161,  // 0x9A LATIN SMALL LETTER S WITH CARON
    0x203A,  // 0x9B SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    0x0153,  // 0x9C LATIN SMALL LIGATURE OE
    0x009D,  // 0x9D WHATWG 未定义 → Latin-1 恒等（C1 control）
    0x017E,  // 0x9E LATIN SMALL LETTER Z WITH CARON
    0x0178,  // 0x9F LATIN CAPITAL LETTER Y WITH DIAERESIS
};

// 差异表的编码侧反查表（码点 → byte 0x80-0x9F），按码点升序，共 27 项。
constexpr u32 CP1252_ENCODE_CP[27] = {
    0x0152, 0x0153, 0x0160, 0x0161, 0x0178, 0x017D, 0x017E, 0x0192, 0x02C6,
    0x02DC, 0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E,
    0x2020, 0x2021, 0x2022, 0x2026, 0x2030, 0x2039, 0x203A, 0x20AC, 0x2122,
};
constexpr u8 CP1252_ENCODE_BYTE[27] = {
    0x8C, 0x9C, 0x8A, 0x9A, 0x9F, 0x8E, 0x9E, 0x83, 0x88,
    0x98, 0x96, 0x97, 0x91, 0x92, 0x82, 0x93, 0x94, 0x84,
    0x86, 0x87, 0x95, 0x85, 0x89, 0x8B, 0x9B, 0x80, 0x99,
};

// 差异码点 → Windows-1252 字节；不在表中返回 0（0 不可能是差异项的编码）。
u8 cp1252_encode_high(u32 cp)
{
    for (usize i = 0; i < 27; ++i) {
        if (CP1252_ENCODE_CP[i] == cp)
            return CP1252_ENCODE_BYTE[i];
    }
    return 0;
}

// 单字节编码（Latin-1 / Windows-1252 共用骨架）→ UTF-8：解码恒成功。
core::StatusResult<std::string> single_byte_to_utf8(std::string_view input)
{
    std::string utf8;
    utf8.reserve(input.size());
    u8 buf[4];
    for (char ch : input) {
        const u8  b  = static_cast<u8>(ch);
        const u16 cp = (b >= 0x80 && b <= 0x9F)
                           ? CP1252_HIGH_TABLE[b - 0x80]
                           : static_cast<u16>(b);
        // Latin-1 / CP1252 解码产物恒为合法标量值，utf8_encode_code_point 不会返回 0。
        const usize n = utf8_encode_code_point(cp, buf);
        utf8.append(reinterpret_cast<const char*>(buf), n);
    }
    return core::Ok<std::string>(std::move(utf8));
}

// UTF-8 → 单字节编码（Latin-1 / Windows-1252 共用骨架）。
// allow_cp1252_high 时差异码点回映射到 0x80-0x9F，否则 >U+00FF 一律不可表示。
core::StatusResult<std::string> utf8_to_single_byte(const char*      charset,
                                                    std::string_view utf8,
                                                    bool             allow_cp1252_high)
{
    const u8*  data = reinterpret_cast<const u8*>(utf8.data());
    const usize size = utf8.size();

    std::string out;
    out.reserve(size);
    usize pos = 0;
    while (pos < size) {
        u32 cp = 0;
        if (!utf8_next_code_point(data, size, pos, cp))
            return core::Err(charset_invalid_sequence(charset, pos));
        if (cp <= 0x00FF) {
            out.push_back(static_cast<char>(static_cast<u8>(cp)));
        } else if (allow_cp1252_high) {
            const u8 b = cp1252_encode_high(cp);
            if (b == 0)
                return core::Err(charset_unrepresentable(charset, cp));
            out.push_back(static_cast<char>(b));
        } else {
            return core::Err(charset_unrepresentable(charset, cp));
        }
    }
    return core::Ok<std::string>(std::move(out));
}

}  // namespace

// ============================================================================
// UTF 家族
// ============================================================================

core::StatusResult<std::wstring> utf8_to_wide(std::string_view utf8)
{
    if (utf8.empty())
        return core::Ok<std::wstring>(std::wstring{});
#if defined(_WIN32)
    return utf8_to_wide_via_utf16(utf8);
#else
    return utf8_to_wide_via_ucs4(utf8);
#endif
}

core::StatusResult<std::string> wide_to_utf8(std::wstring_view wide)
{
    if (wide.empty())
        return core::Ok<std::string>(std::string{});
#if defined(_WIN32)
    return wide_to_utf8_via_utf16(wide);
#else
    return wide_to_utf8_via_ucs4(wide);
#endif
}

// ============================================================================
// Latin-1（ISO-8859-1）
// ============================================================================

core::StatusResult<std::string> latin1_to_utf8(std::string_view latin1)
{
    // 注意：必须限定 ca::str:: 前缀，否则无限定名字会命中本 detail::latin1_to_utf8。
    std::string utf8(ca::str::latin1_to_utf8_length(reinterpret_cast<const u8*>(latin1.data()),
                                                    latin1.size()),
                     '\0');
    ca::str::latin1_to_utf8(reinterpret_cast<const u8*>(latin1.data()), latin1.size(),
                            reinterpret_cast<u8*>(utf8.data()));
    return core::Ok<std::string>(std::move(utf8));
}

core::StatusResult<std::string> utf8_to_latin1(std::string_view utf8)
{
    return utf8_to_single_byte("iso-8859-1", utf8, false);
}

core::StatusResult<std::wstring> latin1_to_wide(std::string_view latin1)
{
    std::wstring wide;
    wide.reserve(latin1.size());
    for (char ch : latin1)
        wide.push_back(static_cast<wchar_t>(static_cast<u8>(ch)));
    return core::Ok<std::wstring>(std::move(wide));
}

core::StatusResult<std::string> wide_to_latin1(std::wstring_view wide)
{
    std::string out;
    out.reserve(wide.size());
    for (wchar_t ch : wide) {
        const u32 cp = static_cast<u32>(ch);
        if (cp > 0x00FF)
            return core::Err(charset_unrepresentable("iso-8859-1", cp));
        out.push_back(static_cast<char>(static_cast<u8>(cp)));
    }
    return core::Ok<std::string>(std::move(out));
}

// ============================================================================
// Windows-1252
// ============================================================================

core::StatusResult<std::string> cp1252_to_utf8(std::string_view cp1252)
{
    return single_byte_to_utf8(cp1252);
}

core::StatusResult<std::string> utf8_to_cp1252(std::string_view utf8)
{
    return utf8_to_single_byte("windows-1252", utf8, true);
}

core::StatusResult<std::wstring> cp1252_to_wide(std::string_view cp1252)
{
    std::wstring wide;
    wide.reserve(cp1252.size());
    for (char ch : cp1252) {
        const u8  b  = static_cast<u8>(ch);
        const u16 cp = (b >= 0x80 && b <= 0x9F) ? CP1252_HIGH_TABLE[b - 0x80]
                                                : static_cast<u16>(b);
        wide.push_back(static_cast<wchar_t>(cp));
    }
    return core::Ok<std::wstring>(std::move(wide));
}

core::StatusResult<std::string> wide_to_cp1252(std::wstring_view wide)
{
    std::string out;
    out.reserve(wide.size());
    for (wchar_t ch : wide) {
        const u32 cp = static_cast<u32>(ch);
        if (cp <= 0x00FF) {
            out.push_back(static_cast<char>(static_cast<u8>(cp)));
        } else {
            const u8 b = cp1252_encode_high(cp);
            if (b == 0)
                return core::Err(charset_unrepresentable("windows-1252", cp));
            out.push_back(static_cast<char>(b));
        }
    }
    return core::Ok<std::string>(std::move(out));
}

}  // namespace ca::str::detail
