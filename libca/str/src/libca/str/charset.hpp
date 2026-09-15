//
// @brief 字符编码转换门面（内置实现优先，iconv/系统代码页回落）。
// @author Canrad
// @date 2026/07/20
//
// 实现分层组织（见 doc/加密与编码内置化方案.md §一）：
//   - Tier 1 内置纯算法：UTF 家族、Latin-1（ISO-8859-1）、Windows-1252，
//     零平台与外部依赖，跨平台行为一致；
//   - 本地 ANSI 代码页（local_*）仍跟随系统环境（Windows CP_ACP / POSIX locale）；
//   - GBK 系列暂沿用系统实现（Windows 代码页 / POSIX iconv），后续提交内置化。
//
// 所有方法对非法输入序列返回 `INVALID_ARGUMENT`，不做静默字节替换。

#pragma once

#include "libca/core/status.hpp"

#include <string>
#include <string_view>

namespace ca::str {

/// @brief 代码页字符编码转换工具（纯静态）。
///
/// 提供本地 ANSI 代码页（Windows 为 CP_ACP，POSIX 为当前 locale 的 codeset）
/// 和 GBK（Windows 为 CP_936，POSIX 为 iconv "GBK"）与 UTF-8 / `std::wstring`
/// 之间的双向转换；UTF 家族与 Latin-1 / Windows-1252 为内置纯算法实现。
/// 所有方法均为静态函数，类不可实例化。
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
    /// @note 内置纯算法实现（UTF-8 → UTF-16/UCS-4），不依赖系统代码页。
    static core::StatusResult<std::wstring> utf8_to_wide(std::string_view utf8);

    /// @brief `std::wstring`（Windows 为 UTF-16LE，Linux 为 UCS-4）转 UTF-8 字符串。
    /// @param wide 输入 wchar 序列，**必须是合法编码**（不允许孤立代理项或超出
    ///             Unicode 标量值范围的码点），否则返回 `INVALID_ARGUMENT`。
    /// @note 内置纯算法实现，不依赖系统代码页。
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

    /// @brief ISO-8859-1（Latin-1）字符串转 UTF-8。字节 0x00-0xFF 直映射码点 U+0000-U+00FF。
    /// @note 内置纯算法实现，恒成功。
    static core::StatusResult<std::string> latin1_to_utf8(std::string_view latin1);

    /// @brief UTF-8 字符串转 ISO-8859-1（Latin-1）。
    /// @note 含 >U+00FF 码点时返回 `INVALID_ARGUMENT`（不可表示，不替换）。
    static core::StatusResult<std::string> utf8_to_latin1(std::string_view utf8);

    /// @brief ISO-8859-1（Latin-1）字符串转 `std::wstring`。恒成功。
    static core::StatusResult<std::wstring> latin1_to_wide(std::string_view latin1);

    /// @brief `std::wstring` 转 ISO-8859-1（Latin-1）。
    /// @note 含 >U+00FF 码点时返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::string> wide_to_latin1(std::wstring_view wide);

    /// @brief Windows-1252（cp1252）字符串转 UTF-8。
    /// @note Windows-1252 = Latin-1 + 0x80-0x9F 区段的 27 项差异（WHATWG
    ///       index-windows-1252）；0x81/0x8D/0x8F/0x90/0x9D 五个 WHATWG 未定义字节
    ///       按 Latin-1 恒等映射为对应 C1 control（同 Windows best-fit；WHATWG
    ///       解码器与 python cp1252 对这 5 字节报错，此处取全映射双射，见设计文档）。
    static core::StatusResult<std::string> cp1252_to_utf8(std::string_view cp1252);

    /// @brief UTF-8 字符串转 Windows-1252（cp1252）。
    /// @note 含不可表示码点时返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::string> utf8_to_cp1252(std::string_view utf8);

    /// @brief Windows-1252（cp1252）字符串转 `std::wstring`。
    static core::StatusResult<std::wstring> cp1252_to_wide(std::string_view cp1252);

    /// @brief `std::wstring` 转 Windows-1252（cp1252）。
    /// @note 含不可表示码点时返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::string> wide_to_cp1252(std::wstring_view wide);
};

}  // namespace ca::str
