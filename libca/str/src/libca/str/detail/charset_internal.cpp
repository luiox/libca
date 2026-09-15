//
// @brief 字符编码内置实现内部共享助手。
// @author Canrad
// @date 2026/09/15
//

#include "libca/str/detail/charset_internal.hpp"

#include "libca/str/char_util.hpp"
#include "libca/str/format.hpp"
#include "libca/str/utf8_util.hpp"

namespace ca::str::detail {

core::Status charset_invalid_sequence(const char* charset, usize pos)
{
    return core::ErrStatus(core::StatusCode::INVALID_ARGUMENT,
                           ca::str::format_std("{}: invalid sequence at byte {}", charset, pos));
}

core::Status charset_unrepresentable(const char* charset, u32 cp)
{
    return core::ErrStatus(
        core::StatusCode::INVALID_ARGUMENT,
        ca::str::format_std("{}: code point U+{:04X} is not representable", charset,
                            static_cast<unsigned long>(cp)));
}

bool utf8_next_code_point(const u8* data, usize size, usize& pos, u32& cp_out)
{
    const usize clen = utf8_code_point_bytes(data[pos]);
    if (clen == 0 || pos + clen > size || !utf8_valid_continuation(data + pos, clen))
        return false;
    const u32 cp = utf8_decode_code_point(data + pos);
    // 拒绝代理项码点（含 CESU-8 式 ED A0 80 编码），全平台口径一致。
    if (cp >= 0xD800 && cp <= 0xDFFF)
        return false;
    cp_out = cp;
    pos += clen;
    return true;
}

core::StatusResult<std::u32string> wide_to_code_points(std::wstring_view wide)
{
    std::u32string cps;
    cps.reserve(wide.size());
#if defined(_WIN32)
    // Windows：wchar_t 为 UTF-16 码元，须做代理对配对校验。
    const usize count = wide.size();
    for (usize i = 0; i < count; ++i) {
        const u16 unit = static_cast<u16>(wide[i]);
        if (Utf16Char(unit).is_lead_surrogate()) {
            if (i + 1 >= count || !Utf16Char(static_cast<u16>(wide[i + 1])).is_trail_surrogate())
                return core::Err(charset_invalid_sequence("wchar", i));
            cps.push_back(Utf16Char::decode_pair(Utf16Char(unit),
                                                 Utf16Char(static_cast<u16>(wide[i + 1]))));
            ++i;
        } else if (Utf16Char(unit).is_trail_surrogate()) {
            return core::Err(charset_invalid_sequence("wchar", i));
        } else {
            cps.push_back(unit);
        }
    }
#else
    // POSIX：wchar_t 为 UCS-4，逐码点校验标量值范围（glibc iconv 接受超上限
    // 码点的历史差异就此消除，见单测说明）。
    for (wchar_t ch : wide) {
        const u32 cp = static_cast<u32>(ch);
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return core::Err(charset_unrepresentable("wchar", cp));
        cps.push_back(cp);
    }
#endif
    return core::Ok<std::u32string>(std::move(cps));
}

void wide_push_code_point(std::wstring& out, u32 cp)
{
#if defined(_WIN32)
    if (cp >= 0x10000) {
        Utf16Char high, low;
        Utf16Char::encode_pair(cp, high, low);
        out.push_back(static_cast<wchar_t>(high.unit()));
        out.push_back(static_cast<wchar_t>(low.unit()));
    } else {
        out.push_back(static_cast<wchar_t>(cp));
    }
#else
    out.push_back(static_cast<wchar_t>(cp));
#endif
}

}  // namespace ca::str::detail
