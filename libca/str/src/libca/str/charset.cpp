//
// @brief 字符编码转换门面实现（三级查找：内置表 → iconv 回落 → UNSUPPORTED）
// @author Canrad
// @date 2026/07/20
//
// 分支结构：
//   - Windows：UTF 家族 / Latin-1 / Windows-1252 / GB18030 全部内置；
//     仅本地 ANSI 代码页（CP_ACP）走 Win32 MultiByteToWideChar。
//   - POSIX（--with_iconv=y，默认）：同上全部内置；本地代码页与长尾编码走 iconv。
//   - POSIX（--with_iconv=n，纯内置构建）：不引用 iconv 头；本地代码页仅在
//     codeset 为 UTF-8 / ASCII / Latin-1 时可用（走内置），否则 UNIMPLEMENTED。
//
// POSIX iconv 语义备忘：本地 codeset 取 nl_langinfo(CODESET)，GBK 曾用 "GBK"
// 转换器（现改内置表）；wchar 用 "WCHAR_T"。错误对齐：非法序列 INVALID_ARGUMENT，
// 转换对不存在（如裁剪 gconv）UNIMPLEMENTED。

#include "charset.hpp"

#include "libca/str/detail/charset_builtin.hpp"
#include "libca/str/detail/charset_gb18030.hpp"
#include "libca/str/format.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>

#    include <climits>
#elif !defined(LIBCA_STR_NO_ICONV)
#    include <algorithm>
#    include <cerrno>
#    include <cstring>
#    include <iconv.h>
#    include <langinfo.h>
#else
#    include <cstring>
#    include <langinfo.h>
#endif

#include <algorithm>
#include <array>

namespace ca::str {

namespace {

// ============================================================================
// 编码别名表（supported / list_supported 共用）。
// ============================================================================

// 内置覆盖的编码名（小写；alias 为接受名，canonical 为该组代表名）。
// "gbk" 组按 GB18030 处理（GBK 是其双字节子集），见 charset.hpp 类注释。
struct CharsetAlias {
    std::string_view alias;
    std::string_view canonical;
};

constexpr std::array<CharsetAlias, 12> kBuiltinCharsets{{
    {"utf-8", "utf-8"},
    {"utf8", "utf-8"},
    {"gb18030", "gb18030"},
    {"gbk", "gb18030"},
    {"cp936", "gb18030"},
    {"gb2312", "gb18030"},
    {"iso-8859-1", "iso-8859-1"},
    {"latin-1", "iso-8859-1"},
    {"latin1", "iso-8859-1"},
    {"windows-1252", "windows-1252"},
    {"cp1252", "windows-1252"},
    {"local", "local"},
}};

// ASCII 小写归一化（只处理 <0x80，多字节输入原样保留参与比较，不会误折叠）。
std::string normalize_charset_name(std::string_view charset)
{
    std::string name;
    name.reserve(charset.size());
    for (char ch : charset) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        name.push_back(static_cast<char>((uc < 0x80 && uc >= 'A' && uc <= 'Z')
                                             ? static_cast<unsigned char>(uc - 'A' + 'a')
                                             : uc));
    }
    return name;
}

// 内置别名表查询：命中返回规范名。
const std::string_view* lookup_builtin_charset(std::string_view charset)
{
    if (charset.empty())
        return nullptr;
    const std::string name = normalize_charset_name(charset);
    for (const auto& entry : kBuiltinCharsets) {
        if (entry.alias == name)
            return &entry.canonical;
    }
    return nullptr;
}

#if !defined(_WIN32) && !defined(LIBCA_STR_NO_ICONV)
// Tier 3 探测：iconv 是否提供该 codeset 与 UTF-8 的转换对。
bool iconv_supports(std::string_view charset)
{
    const std::string name(charset);
    iconv_t cd = ::iconv_open("UTF-8", name.c_str());
    if (cd == reinterpret_cast<iconv_t>(-1))
        return false;
    ::iconv_close(cd);
    return true;
}
#endif

}  // namespace

bool CharsetConverter::supported(std::string_view charset)
{
    // 空名在任何平台都不算受支持：glibc 的 iconv_open 对空 tocode 会回落到当前
    // locale 字符集而成功，探测路径因此必须先行排除（否则 Windows/POSIX 口径分叉）。
    if (charset.empty())
        return false;
    if (lookup_builtin_charset(charset) != nullptr)
        return true;
#if !defined(_WIN32) && !defined(LIBCA_STR_NO_ICONV)
    return iconv_supports(charset);
#else
    return false;  // Windows / 纯内置构建：内置表之外不提供长尾编码
#endif
}

std::vector<std::string> CharsetConverter::list_supported()
{
    // 返回全部接受名（含别名），按字典序；kBuiltinCharsets 本身已按名排好。
    std::vector<std::string> names;
    names.reserve(kBuiltinCharsets.size());
    for (const auto& entry : kBuiltinCharsets)
        names.emplace_back(entry.alias);
    std::sort(names.begin(), names.end());
    return names;
}

#if defined(_WIN32)

// ============================================================================
// Windows：Win32 API 仅剩本地 ANSI 代码页路径，其余全部内置。
// ============================================================================

namespace {

// 把 Win32 转换 API 的 "0 = 失败" 翻译为 Status。
core::Status wide_convert_error(const char* operation)
{
    const DWORD error = GetLastError();
    return core::ErrStatus(
        error == ERROR_NO_UNICODE_TRANSLATION ? core::StatusCode::INVALID_ARGUMENT
                                              : core::StatusCode::INTERNAL,
        ca::str::format_std("{} failed with Windows error {}",
                            operation,
                            static_cast<unsigned long>(error)));
}

// 多字节（CP_ACP）→ 宽字符：先探测输出大小，再真正转换。
core::StatusResult<std::wstring> ansi_to_wide(std::string_view input)
{
    if (input.empty())
        return core::Ok<std::wstring>(std::wstring{});
    if (input.size() > static_cast<usize>(INT_MAX))
        return core::Err(core::ErrStatus(core::StatusCode::INVALID_ARGUMENT,
                                         "MultiByteToWideChar input exceeds the Win32 API length limit"));

    const int length = MultiByteToWideChar(CP_ACP, 0, input.data(),
                                           static_cast<int>(input.size()), nullptr, 0);
    if (length <= 0)
        return core::Err(wide_convert_error("MultiByteToWideChar"));

    std::wstring converted(static_cast<usize>(length), L'\0');
    if (MultiByteToWideChar(CP_ACP, 0, input.data(), static_cast<int>(input.size()),
                            &converted[0], length)
        == 0)
        return core::Err(wide_convert_error("MultiByteToWideChar"));
    return core::Ok<std::wstring>(std::move(converted));
}

// 宽字符 → 多字节（CP_ACP）。WC_ERR_INVALID_CHARS 对 CP_ACP 会被 Win32 拒绝
// （ERROR_INVALID_FLAGS），故传 0：无法表示的码点由系统做 best-fit 替换。
core::StatusResult<std::string> wide_to_ansi(std::wstring_view input)
{
    if (input.empty())
        return core::Ok<std::string>(std::string{});
    if (input.size() > static_cast<usize>(INT_MAX))
        return core::Err(core::ErrStatus(core::StatusCode::INVALID_ARGUMENT,
                                         "WideCharToMultiByte input exceeds the Win32 API length limit"));

    const int length = WideCharToMultiByte(CP_ACP, 0, input.data(),
                                           static_cast<int>(input.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (length <= 0)
        return core::Err(wide_convert_error("WideCharToMultiByte"));

    std::string converted(static_cast<usize>(length), '\0');
    if (WideCharToMultiByte(CP_ACP, 0, input.data(), static_cast<int>(input.size()),
                            &converted[0], length, nullptr, nullptr)
        == 0)
        return core::Err(wide_convert_error("WideCharToMultiByte"));
    return core::Ok<std::string>(std::move(converted));
}

}  // namespace

core::StatusResult<std::wstring> CharsetConverter::utf8_to_wide(std::string_view utf8)
{
    return detail::utf8_to_wide(utf8);
}

core::StatusResult<std::string> CharsetConverter::wide_to_utf8(std::wstring_view wide)
{
    return detail::wide_to_utf8(wide);
}

core::StatusResult<std::wstring> CharsetConverter::local_to_wide(std::string_view local)
{
    return ansi_to_wide(local);
}

core::StatusResult<std::string> CharsetConverter::local_to_utf8(std::string_view local)
{
    auto wide = local_to_wide(local);
    if (wide.is_err())
        return core::Err(wide.unwrap_err());
    return wide_to_utf8(std::move(wide).unwrap());
}

core::StatusResult<std::string> CharsetConverter::gbk_to_utf8(std::string_view gbk)
{
    return detail::gb18030_to_utf8(gbk);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gbk(std::string_view utf8)
{
    return detail::utf8_to_gb18030(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::gbk_to_wide(std::string_view gbk)
{
    return detail::gb18030_to_wide(gbk);
}

core::StatusResult<std::string> CharsetConverter::wide_to_gbk(std::wstring_view wide)
{
    return detail::wide_to_gb18030(wide);
}

core::StatusResult<std::string> CharsetConverter::gb18030_to_utf8(std::string_view gb18030)
{
    return detail::gb18030_to_utf8(gb18030);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gb18030(std::string_view utf8)
{
    return detail::utf8_to_gb18030(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::gb18030_to_wide(std::string_view gb18030)
{
    return detail::gb18030_to_wide(gb18030);
}

core::StatusResult<std::string> CharsetConverter::wide_to_gb18030(std::wstring_view wide)
{
    return detail::wide_to_gb18030(wide);
}

core::StatusResult<std::string> CharsetConverter::latin1_to_utf8(std::string_view latin1)
{
    return detail::latin1_to_utf8(latin1);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_latin1(std::string_view utf8)
{
    return detail::utf8_to_latin1(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::latin1_to_wide(std::string_view latin1)
{
    return detail::latin1_to_wide(latin1);
}

core::StatusResult<std::string> CharsetConverter::wide_to_latin1(std::wstring_view wide)
{
    return detail::wide_to_latin1(wide);
}

core::StatusResult<std::string> CharsetConverter::cp1252_to_utf8(std::string_view cp1252)
{
    return detail::cp1252_to_utf8(cp1252);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_cp1252(std::string_view utf8)
{
    return detail::utf8_to_cp1252(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::cp1252_to_wide(std::string_view cp1252)
{
    return detail::cp1252_to_wide(cp1252);
}

core::StatusResult<std::string> CharsetConverter::wide_to_cp1252(std::wstring_view wide)
{
    return detail::wide_to_cp1252(wide);
}

#elif !defined(LIBCA_STR_NO_ICONV)

// ============================================================================
// POSIX + iconv：长尾与本地代码页回落 iconv，其余内置。
// ============================================================================

namespace {

// iconv_open 失败：EINVAL 表示系统没有该转换对（如缺 gconv 模块）。
core::Status iconv_open_error(const char* from, const char* to)
{
    if (errno == EINVAL)
        return core::ErrStatus(
            core::StatusCode::UNIMPLEMENTED,
            ca::str::format_std("iconv has no conversion from {} to {}", from, to));
    return core::ErrStatus(
        core::StatusCode::INTERNAL,
        ca::str::format_std("iconv_open({} -> {}) failed with errno {}", from, to, errno));
}

// iconv 通用转换：from → to，输出为原始字节（宽字符方向由调用方按 sizeof(wchar_t)
// 重解释）。E2BIG 时扩容重试；EILSEQ/EINVAL（非法/残缺序列）按 INVALID_ARGUMENT
// 报错，与内置路径的严格语义一致。
core::StatusResult<std::string> iconv_convert(const char* to, const char* from,
                                              const char* input, usize input_len)
{
    if (input_len == 0)
        return core::Ok<std::string>(std::string{});

    iconv_t cd = ::iconv_open(to, from);
    if (cd == reinterpret_cast<iconv_t>(-1))
        return core::Err(iconv_open_error(from, to));

    usize       out_capacity = std::max<usize>(input_len * 4, 16);
    std::string output(out_capacity, '\0');
    char*       in_cursor  = const_cast<char*>(input);
    usize       in_left    = input_len;
    char*       out_cursor = output.data();
    usize       out_left   = out_capacity;

    // POSIX 规定 iconv 成功返回当且仅当输入耗尽；其余情况（输出满/非法/残缺）
    // 均返回 -1 置 errno，故循环不会空转。
    while (in_left > 0) {
        if (::iconv(cd, &in_cursor, &in_left, &out_cursor, &out_left)
            == static_cast<usize>(-1)) {
            if (errno == E2BIG) {
                const usize used = out_capacity - out_left;
                out_capacity *= 2;
                output.resize(out_capacity);
                out_cursor = output.data() + used;
                out_left   = out_capacity - used;
                continue;
            }
            const core::StatusCode code = (errno == EILSEQ || errno == EINVAL)
                                              ? core::StatusCode::INVALID_ARGUMENT
                                              : core::StatusCode::INTERNAL;
            ::iconv_close(cd);
            return core::Err(core::ErrStatus(
                code,
                ca::str::format_std(
                    "iconv from {} to {} failed with errno {}", from, to, errno)));
        }
    }
    ::iconv_close(cd);
    output.resize(out_capacity - out_left);
    return core::Ok<std::string>(std::move(output));
}

// iconv 的 "WCHAR_T" 输出按本机 wchar_t 重解释为 std::wstring。
core::StatusResult<std::wstring> bytes_to_wide(std::string bytes)
{
    if (bytes.size() % sizeof(wchar_t) != 0) {
        return core::Err(core::ErrStatus(
            core::StatusCode::INTERNAL, "iconv produced non-wchar-aligned output"));
    }
    std::wstring wide(bytes.size() / sizeof(wchar_t), L'\0');
    if (!wide.empty())
        std::memcpy(wide.data(), bytes.data(), bytes.size());
    return core::Ok<std::wstring>(std::move(wide));
}

// 当前 locale 的 codeset，等价 Windows 的「本地 ANSI 代码页」语义。
// 注意：C 程序启动时 locale 恒为 "C"（codeset 为 ASCII）；调用方需要跟随
// 环境时应先 setlocale(LC_ALL, "")。
const char* local_codeset()
{
    return ::nl_langinfo(CODESET);
}

const char* wide_input_bytes(const wchar_t* data)
{
    return reinterpret_cast<const char*>(data);
}

usize wide_input_size(std::wstring_view wide)
{
    return wide.size() * sizeof(wchar_t);
}

}  // namespace

core::StatusResult<std::wstring> CharsetConverter::utf8_to_wide(std::string_view utf8)
{
    // 内置纯算法：UTF-8 → UCS-4，不再经 iconv（glibc 接受超 Unicode 上限码点的
    // 差异就此消除，见单测 InvalidWideCodePointRejected）。
    return detail::utf8_to_wide(utf8);
}

core::StatusResult<std::string> CharsetConverter::wide_to_utf8(std::wstring_view wide)
{
    // 内置纯算法：UCS-4 → UTF-8，不再经 iconv。
    return detail::wide_to_utf8(wide);
}

core::StatusResult<std::wstring> CharsetConverter::local_to_wide(std::string_view local)
{
    auto bytes = iconv_convert("WCHAR_T", local_codeset(), local.data(), local.size());
    if (bytes.is_err())
        return core::Err(bytes.unwrap_err());
    return bytes_to_wide(std::move(bytes).unwrap());
}

core::StatusResult<std::string> CharsetConverter::local_to_utf8(std::string_view local)
{
    return iconv_convert("UTF-8", local_codeset(), local.data(), local.size());
}

core::StatusResult<std::string> CharsetConverter::gbk_to_utf8(std::string_view gbk)
{
    // 内置 GB18030 表驱动：不再依赖 gconv 的 GBK 模块（裁剪 glibc 环境的痛点）。
    return detail::gb18030_to_utf8(gbk);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gbk(std::string_view utf8)
{
    return detail::utf8_to_gb18030(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::gbk_to_wide(std::string_view gbk)
{
    return detail::gb18030_to_wide(gbk);
}

core::StatusResult<std::string> CharsetConverter::wide_to_gbk(std::wstring_view wide)
{
    return detail::wide_to_gb18030(wide);
}

core::StatusResult<std::string> CharsetConverter::gb18030_to_utf8(std::string_view gb18030)
{
    return detail::gb18030_to_utf8(gb18030);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gb18030(std::string_view utf8)
{
    return detail::utf8_to_gb18030(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::gb18030_to_wide(std::string_view gb18030)
{
    return detail::gb18030_to_wide(gb18030);
}

core::StatusResult<std::string> CharsetConverter::wide_to_gb18030(std::wstring_view wide)
{
    return detail::wide_to_gb18030(wide);
}

core::StatusResult<std::string> CharsetConverter::latin1_to_utf8(std::string_view latin1)
{
    return detail::latin1_to_utf8(latin1);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_latin1(std::string_view utf8)
{
    return detail::utf8_to_latin1(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::latin1_to_wide(std::string_view latin1)
{
    return detail::latin1_to_wide(latin1);
}

core::StatusResult<std::string> CharsetConverter::wide_to_latin1(std::wstring_view wide)
{
    return detail::wide_to_latin1(wide);
}

core::StatusResult<std::string> CharsetConverter::cp1252_to_utf8(std::string_view cp1252)
{
    return detail::cp1252_to_utf8(cp1252);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_cp1252(std::string_view utf8)
{
    return detail::utf8_to_cp1252(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::cp1252_to_wide(std::string_view cp1252)
{
    return detail::cp1252_to_wide(cp1252);
}

core::StatusResult<std::string> CharsetConverter::wide_to_cp1252(std::wstring_view wide)
{
    return detail::wide_to_cp1252(wide);
}

#else

// ============================================================================
// POSIX 纯内置构建（--with_iconv=n）：不引用 iconv。
// 本地代码页仅在 codeset 为 UTF-8 / ASCII / Latin-1 时走内置，其余 UNIMPLEMENTED。
// ============================================================================

namespace {

// 当前 locale 的 codeset（libc 自带，不依赖 iconv）。
const char* local_codeset()
{
    return ::nl_langinfo(CODESET);
}

// 纯内置构建下本地 codeset 的内置覆盖判定与归一化。
enum class LocalCodeset { Utf8, Ascii, Latin1, Unsupported };

LocalCodeset classify_local_codeset(const char* codeset)
{
    const std::string name = normalize_charset_name(codeset);
    if (name == "utf-8" || name == "utf8")
        return LocalCodeset::Utf8;
    if (name == "ansi_x3.4-1968" || name == "us-ascii" || name == "ascii")
        return LocalCodeset::Ascii;
    if (name == "iso-8859-1" || name == "iso8859-1" || name == "latin1")
        return LocalCodeset::Latin1;
    return LocalCodeset::Unsupported;
}

core::Status local_unsupported(const char* codeset)
{
    return core::ErrStatus(
        core::StatusCode::UNIMPLEMENTED,
        ca::str::format_std(
            "local codeset '{}' is not built-in; rebuild with --with_iconv=y for long-tail support",
            codeset));
}

}  // namespace

core::StatusResult<std::wstring> CharsetConverter::utf8_to_wide(std::string_view utf8)
{
    return detail::utf8_to_wide(utf8);
}

core::StatusResult<std::string> CharsetConverter::wide_to_utf8(std::wstring_view wide)
{
    return detail::wide_to_utf8(wide);
}

core::StatusResult<std::wstring> CharsetConverter::local_to_wide(std::string_view local)
{
    const char* codeset = local_codeset();
    switch (classify_local_codeset(codeset)) {
    case LocalCodeset::Utf8:
        return detail::utf8_to_wide(local);
    case LocalCodeset::Ascii:
    case LocalCodeset::Latin1: {
        // ASCII 是 Latin-1 的子集：字节直映射 wchar。
        std::wstring wide;
        wide.reserve(local.size());
        for (char ch : local)
            wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
        return core::Ok<std::wstring>(std::move(wide));
    }
    case LocalCodeset::Unsupported:
    default:
        return core::Err(local_unsupported(codeset));
    }
}

core::StatusResult<std::string> CharsetConverter::local_to_utf8(std::string_view local)
{
    const char* codeset = local_codeset();
    switch (classify_local_codeset(codeset)) {
    case LocalCodeset::Utf8:
        return core::Ok<std::string>(std::string(local));
    case LocalCodeset::Ascii:
        // ASCII 严格版：>0x7F 的字节非法。
        for (usize i = 0; i < local.size(); ++i) {
            if (static_cast<unsigned char>(local[i]) > 0x7F)
                return core::Err(core::ErrStatus(
                    core::StatusCode::INVALID_ARGUMENT,
                    ca::str::format_std("ascii: invalid sequence at byte {}",
                                        static_cast<unsigned long>(i))));
        }
        return core::Ok<std::string>(std::string(local));
    case LocalCodeset::Latin1:
        return detail::latin1_to_utf8(local);
    case LocalCodeset::Unsupported:
    default:
        return core::Err(local_unsupported(codeset));
    }
}

core::StatusResult<std::string> CharsetConverter::gbk_to_utf8(std::string_view gbk)
{
    return detail::gb18030_to_utf8(gbk);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gbk(std::string_view utf8)
{
    return detail::utf8_to_gb18030(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::gbk_to_wide(std::string_view gbk)
{
    return detail::gb18030_to_wide(gbk);
}

core::StatusResult<std::string> CharsetConverter::wide_to_gbk(std::wstring_view wide)
{
    return detail::wide_to_gb18030(wide);
}

core::StatusResult<std::string> CharsetConverter::gb18030_to_utf8(std::string_view gb18030)
{
    return detail::gb18030_to_utf8(gb18030);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gb18030(std::string_view utf8)
{
    return detail::utf8_to_gb18030(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::gb18030_to_wide(std::string_view gb18030)
{
    return detail::gb18030_to_wide(gb18030);
}

core::StatusResult<std::string> CharsetConverter::wide_to_gb18030(std::wstring_view wide)
{
    return detail::wide_to_gb18030(wide);
}

core::StatusResult<std::string> CharsetConverter::latin1_to_utf8(std::string_view latin1)
{
    return detail::latin1_to_utf8(latin1);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_latin1(std::string_view utf8)
{
    return detail::utf8_to_latin1(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::latin1_to_wide(std::string_view latin1)
{
    return detail::latin1_to_wide(latin1);
}

core::StatusResult<std::string> CharsetConverter::wide_to_latin1(std::wstring_view wide)
{
    return detail::wide_to_latin1(wide);
}

core::StatusResult<std::string> CharsetConverter::cp1252_to_utf8(std::string_view cp1252)
{
    return detail::cp1252_to_utf8(cp1252);
}

core::StatusResult<std::string> CharsetConverter::utf8_to_cp1252(std::string_view utf8)
{
    return detail::utf8_to_cp1252(utf8);
}

core::StatusResult<std::wstring> CharsetConverter::cp1252_to_wide(std::string_view cp1252)
{
    return detail::cp1252_to_wide(cp1252);
}

core::StatusResult<std::string> CharsetConverter::wide_to_cp1252(std::wstring_view wide)
{
    return detail::wide_to_cp1252(wide);
}

#endif  // platform branches

}  // namespace ca::str
