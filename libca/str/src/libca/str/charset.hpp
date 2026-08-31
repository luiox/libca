// Windows 代码页（GBK / 本地 ANSI）与 UTF-8 / wchar 互转工具。
// Windows 实现基于 Win32 `MultiByteToWideChar` / `WideCharToMultiByte`；
// POSIX 实现基于 iconv（GBK 用 "GBK" 转换器，本地代码页取当前 locale 的
// codeset，wchar 用 "WCHAR_T"）。转换对不被系统支持时返回 `UNIMPLEMENTED`
// （如裁剪过的 glibc 缺 GBK gconv 模块）。

#pragma once

#include "libca/core/status.hpp"

#include <string>
#include <string_view>

namespace ca::str {

/// @brief 代码页字符编码转换工具（纯静态）。
///
/// 提供本地 ANSI 代码页（Windows 为 CP_ACP，POSIX 为当前 locale 的 codeset）
/// 和 GBK（Windows 为 CP_936，POSIX 为 iconv "GBK"）与 UTF-8 / `std::wstring`
/// 之间的双向转换。所有方法均为静态函数，类不可实例化。
///
/// @note `std::wstring` 的编码跟随平台：Windows 为 UTF-16LE，Linux 为 UCS-4。
/// @note GBK / GB2312 都经由同一 GBK 转换器处理：GBK 是 GB2312 的超集，
///       CP_936 在现代 Windows 上等价于 GBK，调用方无需区分。
/// @note POSIX 侧「本地代码页」取 `nl_langinfo(CODESET)`：C 程序启动时 locale
///       恒为 "C"（ASCII），需要跟随环境时应先 `setlocale(LC_ALL, "")`。
class CharsetConverter {
public:
    CharsetConverter()  = delete;
    ~CharsetConverter() = delete;
    CharsetConverter(const CharsetConverter&)            = delete;
    CharsetConverter& operator=(const CharsetConverter&) = delete;

    /// @brief UTF-8 字符串转 `std::wstring`（Windows 为 UTF-16LE，Linux 为 UCS-4）。
    /// @param utf8 输入 UTF-8 字节序列，**必须是合法 UTF-8**，否则返回 `INVALID_ARGUMENT`。
    /// @return 转换结果；Windows 上输入超过 Win32 API 长度上限时返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::wstring> utf8_to_wide(std::string_view utf8);

    /// @brief `std::wstring`（Windows 为 UTF-16LE，Linux 为 UCS-4）转 UTF-8 字符串。
    /// @param wide 输入 wchar 序列，**必须是合法编码**，否则返回 `INVALID_ARGUMENT`。
    /// @return 转换结果；Windows 上输入超过 Win32 API 长度上限时返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::string> wide_to_utf8(std::wstring_view wide);

    /// @brief 本地 ANSI 代码页字符串转 `std::wstring`。
    /// @note Windows 为 CP_ACP（系统区域，中文 Windows 通常是 GBK）；
    ///       POSIX 为当前 locale 的 codeset（未 setlocale 时为 ASCII）。
    static core::StatusResult<std::wstring> local_to_wide(std::string_view local);

    /// @brief 本地 ANSI 代码页（CP_ACP）字符串转 UTF-8。等价于 `local_to_wide` + `wide_to_utf8`。
    static core::StatusResult<std::string> local_to_utf8(std::string_view local);

    /// @brief GBK（CP_936）字符串转 UTF-8。
    static core::StatusResult<std::string> gbk_to_utf8(std::string_view gbk);

    /// @brief UTF-8 字符串转 GBK（CP_936）。
    static core::StatusResult<std::string> utf8_to_gbk(std::string_view utf8);

    /// @brief GBK（CP_936）字符串转 `std::wstring`。
    static core::StatusResult<std::wstring> gbk_to_wide(std::string_view gbk);

    /// @brief `std::wstring` 转 GBK（CP_936）。
    static core::StatusResult<std::string> wide_to_gbk(std::wstring_view wide);
};

}  // namespace ca::str
