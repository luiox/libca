//
// @brief 代码页字符编码转换实现（Windows: Win32 API / POSIX: iconv）
// @author Canrad
// @date 2026/07/20
//

#include "charset.hpp"

#include "libca/str/format.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>

#    include <climits>
#else
#    include <algorithm>
#    include <cerrno>
#    include <cstring>
#    include <iconv.h>
#    include <langinfo.h>
#endif

namespace ca::str {

#if defined(_WIN32)

namespace {

// 把 `MultiByteToWideChar` / `WideCharToMultiByte` 的 "0 = 失败" 翻译为 Status。
// 失败时把操作名 + GetLastError() 一并塞进 message，方便排查编码或缓冲问题。
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

core::Status input_too_large(const char* operation)
{
    return core::ErrStatus(core::StatusCode::INVALID_ARGUMENT,
                           ca::str::format_std("{} input exceeds the Win32 API length limit",
                                               operation));
}

// 多字节 → 宽字符的通用实现：先用 0 长度探测输出大小，再分配并真正转换。
// MB_ERR_INVALID_CHARS 让非法序列立即失败而不是被静默丢弃。
// 注意：与 wide_to_multi_byte 同理，该 flag 只对 CP_UTF8 / CP_UTF7 有效；
// 其它代码页（CP_ACP / CP_936 等）传该 flag 会被 Win32 拒绝，回退到 0。
core::StatusResult<std::wstring> multi_byte_to_wide(unsigned int code_page, std::string_view input)
{
    if (input.empty())
        return core::Ok<std::wstring>(std::wstring{});
    if (input.size() > static_cast<usize>(INT_MAX))
        return core::Err(input_too_large("MultiByteToWideChar"));

    const DWORD flags = (code_page == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0;

    const int length = MultiByteToWideChar(static_cast<UINT>(code_page),
                                           flags,
                                           input.data(),
                                           static_cast<int>(input.size()),
                                           nullptr,
                                           0);
    if (length <= 0)
        return core::Err(wide_convert_error("MultiByteToWideChar"));

    std::wstring converted(static_cast<usize>(length), L'\0');
    if (MultiByteToWideChar(static_cast<UINT>(code_page),
                            flags,
                            input.data(),
                            static_cast<int>(input.size()),
                            &converted[0],
                            length) == 0)
        return core::Err(wide_convert_error("MultiByteToWideChar"));

    return core::Ok<std::wstring>(std::move(converted));
}

// 宽字符 → 多字节的通用实现。注意：Win32 文档明确 WC_ERR_INVALID_CHARS 只对
// CP_UTF8 / CP_UTF7 有效，对 CP_ACP / CP_936 等其它代码页传该 flag 会被拒绝
// （GetLastError 返回 ERROR_INVALID_FLAGS = 1004）。因此 UTF-8 路径要求严格输入，
// 其它代码页回退到 0（默认行为，无法表示的码点会被替换字符替代）。
core::StatusResult<std::string> wide_to_multi_byte(unsigned int code_page, std::wstring_view input)
{
    if (input.empty())
        return core::Ok<std::string>(std::string{});
    if (input.size() > static_cast<usize>(INT_MAX))
        return core::Err(input_too_large("WideCharToMultiByte"));

    const DWORD flags = (code_page == CP_UTF8) ? WC_ERR_INVALID_CHARS : 0;

    const int length = WideCharToMultiByte(static_cast<UINT>(code_page),
                                           flags,
                                           input.data(),
                                           static_cast<int>(input.size()),
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
    if (length <= 0)
        return core::Err(wide_convert_error("WideCharToMultiByte"));

    std::string converted(static_cast<usize>(length), '\0');
    if (WideCharToMultiByte(static_cast<UINT>(code_page),
                            flags,
                            input.data(),
                            static_cast<int>(input.size()),
                            &converted[0],
                            length,
                            nullptr,
                            nullptr) == 0)
        return core::Err(wide_convert_error("WideCharToMultiByte"));

    return core::Ok<std::string>(std::move(converted));
}

}  // namespace

core::StatusResult<std::wstring> CharsetConverter::utf8_to_wide(std::string_view utf8)
{
    return multi_byte_to_wide(CP_UTF8, utf8);
}

core::StatusResult<std::string> CharsetConverter::wide_to_utf8(std::wstring_view wide)
{
    return wide_to_multi_byte(CP_UTF8, wide);
}

core::StatusResult<std::wstring> CharsetConverter::local_to_wide(std::string_view local)
{
    return multi_byte_to_wide(CP_ACP, local);
}

core::StatusResult<std::string> CharsetConverter::local_to_utf8(std::string_view local)
{
    auto wide = local_to_wide(local);
    if (wide.is_err())
        return core::Err(wide.unwrap_err());
    return wide_to_multi_byte(CP_UTF8, std::move(wide).unwrap());
}

core::StatusResult<std::string> CharsetConverter::gbk_to_utf8(std::string_view gbk)
{
    auto wide = multi_byte_to_wide(936, gbk);
    if (wide.is_err())
        return core::Err(wide.unwrap_err());
    return wide_to_multi_byte(CP_UTF8, std::move(wide).unwrap());
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gbk(std::string_view utf8)
{
    auto wide = multi_byte_to_wide(CP_UTF8, utf8);
    if (wide.is_err())
        return core::Err(wide.unwrap_err());
    return wide_to_multi_byte(936, std::move(wide).unwrap());
}

core::StatusResult<std::wstring> CharsetConverter::gbk_to_wide(std::string_view gbk)
{
    return multi_byte_to_wide(936, gbk);
}

core::StatusResult<std::string> CharsetConverter::wide_to_gbk(std::wstring_view wide)
{
    return wide_to_multi_byte(936, wide);
}

#else  // !defined(_WIN32)

// POSIX 实现：iconv。代码页语义映射——本地 ANSI（Windows CP_ACP）对应当前
// locale 的 codeset（nl_langinfo(CODESET)），GBK 用 glibc 的 "GBK" 转换器，
// wchar 用 iconv 的 "WCHAR_T"（Linux 上为 UCS-4）。错误语义与 Windows 分支
// 对齐：非法/残缺多字节序列返回 INVALID_ARGUMENT；转换对不被系统支持
// （如裁剪过的 gconv 库缺 GBK）返回 UNIMPLEMENTED。

namespace {

// iconv_open 失败：EINVAL 表示系统没有该转换对（如缺 GBK gconv 模块）。
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
// 报错，与 Win32 分支 MB_ERR_INVALID_CHARS 的严格语义一致。
core::StatusResult<std::string> iconv_convert(const char*        to,
                                              const char*        from,
                                              const char*        input,
                                              usize              input_len)
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
    auto bytes = iconv_convert("WCHAR_T", "UTF-8", utf8.data(), utf8.size());
    if (bytes.is_err())
        return core::Err(bytes.unwrap_err());
    return bytes_to_wide(std::move(bytes).unwrap());
}

core::StatusResult<std::string> CharsetConverter::wide_to_utf8(std::wstring_view wide)
{
    return iconv_convert("UTF-8", "WCHAR_T", wide_input_bytes(wide.data()),
                         wide_input_size(wide));
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
    return iconv_convert("UTF-8", "GBK", gbk.data(), gbk.size());
}

core::StatusResult<std::string> CharsetConverter::utf8_to_gbk(std::string_view utf8)
{
    return iconv_convert("GBK", "UTF-8", utf8.data(), utf8.size());
}

core::StatusResult<std::wstring> CharsetConverter::gbk_to_wide(std::string_view gbk)
{
    auto bytes = iconv_convert("WCHAR_T", "GBK", gbk.data(), gbk.size());
    if (bytes.is_err())
        return core::Err(bytes.unwrap_err());
    return bytes_to_wide(std::move(bytes).unwrap());
}

core::StatusResult<std::string> CharsetConverter::wide_to_gbk(std::wstring_view wide)
{
    return iconv_convert("GBK", "WCHAR_T", wide_input_bytes(wide.data()),
                         wide_input_size(wide));
}

#endif  // defined(_WIN32)

}  // namespace ca::str
