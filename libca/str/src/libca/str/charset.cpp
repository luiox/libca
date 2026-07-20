//
// @brief Windows 代码页字符编码转换实现
// @author Canrad
// @date 2026/07/20
//

#include "charset.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>

#    include <climits>
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
        std::string(operation) + " failed with Windows error " +
            std::to_string(static_cast<unsigned long>(error)));
}

core::Status input_too_large(const char* operation)
{
    return core::ErrStatus(core::StatusCode::INVALID_ARGUMENT,
                           std::string(operation) + " input exceeds the Win32 API length limit");
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

// 非 Windows 平台：函数存在但永远返回 UNIMPLEMENTED，保证头文件可被跨平台代码包含。
// 这样调用方写 `if (auto r = CharsetConverter::gbk_to_utf8(s); r.is_ok()) { ... }`
// 在 Linux 上仍能编译，只是流程会走到错误分支。

namespace {
core::Status unimplemented()
{
    return core::ErrStatus(core::StatusCode::UNIMPLEMENTED,
                           "CharsetConverter is only available on Windows");
}
}  // namespace

core::StatusResult<std::wstring> CharsetConverter::utf8_to_wide(std::string_view)
{
    return core::Err(unimplemented());
}
core::StatusResult<std::string> CharsetConverter::wide_to_utf8(std::wstring_view)
{
    return core::Err(unimplemented());
}
core::StatusResult<std::wstring> CharsetConverter::local_to_wide(std::string_view)
{
    return core::Err(unimplemented());
}
core::StatusResult<std::string> CharsetConverter::local_to_utf8(std::string_view)
{
    return core::Err(unimplemented());
}
core::StatusResult<std::string> CharsetConverter::gbk_to_utf8(std::string_view)
{
    return core::Err(unimplemented());
}
core::StatusResult<std::string> CharsetConverter::utf8_to_gbk(std::string_view)
{
    return core::Err(unimplemented());
}
core::StatusResult<std::wstring> CharsetConverter::gbk_to_wide(std::string_view)
{
    return core::Err(unimplemented());
}
core::StatusResult<std::string> CharsetConverter::wide_to_gbk(std::wstring_view)
{
    return core::Err(unimplemented());
}

#endif  // defined(_WIN32)

}  // namespace ca::str
