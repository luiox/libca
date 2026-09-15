//
// @brief 字符编码转换门面（内置表优先 → iconv 回落 → UNSUPPORTED）。
// @author Canrad
// @date 2026/07/20
//
// 实现按三级查找组织（见 doc/加密与编码内置化方案.md §一）：
//   1. 内置实现（跨平台一致，不依赖系统环境）：
//      - Tier 1 纯算法：UTF 家族（UTF-8 ↔ wchar）、Latin-1、Windows-1252；
//      - Tier 2 表驱动：GB18030（WHATWG index-gb18030 同源码表，覆盖 GBK）；
//   2. iconv 回落（Tier 3，仅 POSIX，构建开关 --with_iconv，默认开）：
//      本地代码页（local_*）与其余长尾编码；
//   3. 均不可用时返回错误（UNIMPLEMENTED / INVALID_ARGUMENT）。
//
// `--with_iconv=n` 可得纯内置构建（POSIX 下完全不引用 iconv 头）。
// 所有方法对非法输入序列返回 `INVALID_ARGUMENT`，不做静默字节替换。

#pragma once

#include "libca/core/status.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ca::str {

/// @brief 字符编码转换工具（纯静态）。
///
/// 提供本地 ANSI 代码页（Windows 为 CP_ACP，POSIX 为当前 locale 的 codeset）、
/// GBK / GB18030、Latin-1、Windows-1252 与 UTF-8 / `std::wstring` 之间的双向转换。
/// 所有方法均为静态函数，类不可实例化。
///
/// @note `std::wstring` 的编码跟随平台：Windows 为 UTF-16LE，Linux 为 UCS-4。
/// @note GBK / GB2312 / GB18030 的关系：GB2312 ⊂ GBK ⊂ GB18030（双字节部分）。
///       gbk_* 系列在内置路径下统一按 GB18030 处理，语义对齐 python
///       `gb18030` codec / WHATWG index-gb18030。与 Windows CP_936 的已知差异
///       已用固定向量在单测钉死，最主要的一条：CP_936 把单字节 0x80 映射为
///       欧元 U+20AC，而 GB18030 中 0x80 是非法字节、欧元的 GB18030 编码为
///       A2 E3；内置路径对 0x80 返回 `INVALID_ARGUMENT`。此外 CP_936 对不可
///       表示码点做 best-fit 替换，内置路径一律报错不替换。
/// @note POSIX 侧「本地代码页」取 `nl_langinfo(CODESET)`：C 程序启动时 locale
///       恒为 "C"（ASCII），需要跟随环境时应先 `setlocale(LC_ALL, "")`。
class CharsetConverter {
public:
    CharsetConverter()  = delete;
    ~CharsetConverter() = delete;
    CharsetConverter(const CharsetConverter&)            = delete;
    CharsetConverter& operator=(const CharsetConverter&) = delete;

    /// @brief 查询指定编码当前是否可转换。名称大小写不敏感。
    ///
    /// 内置覆盖的编码（含别名，见 `list_supported()`）恒为 true；内置之外的长尾
    /// 编码在 POSIX 且 iconv 启用（`--with_iconv=y`）时按 iconv 是否提供该
    /// codeset 探测，其余情况（Windows / 纯内置构建）为 false。
    ///
    /// @param charset 编码名，如 "utf-8"、"GBK"、"gb18030"、"windows-1252"。
    /// @note 本函数只回答可转换性，不改变 `local` 等环境相关编码的实际行为。
    static bool supported(std::string_view charset);

    /// @brief 列出门面保证可用的编码名（含常用别名），按字典序升序返回。
    ///
    /// 覆盖内置编码的规范名与别名，如 "utf-8" / "utf8"、"gb18030" / "gbk" /
    /// "cp936" / "gb2312"、"iso-8859-1" / "latin1"、"windows-1252" / "cp1252"、
    /// "local"。跨平台返回一致（不含 iconv 长尾，长尾请用 `supported()` 探测）。
    static std::vector<std::string> list_supported();

    /// @brief UTF-8 字符串转 `std::wstring`（Windows 为 UTF-16LE，Linux 为 UCS-4）。
    /// @param utf8 输入 UTF-8 字节序列，**必须是合法 UTF-8**，否则返回 `INVALID_ARGUMENT`。
    /// @note 内置纯算法实现（UTF-8 → UTF-16/UCS-4），不依赖系统代码页。
    static core::StatusResult<std::wstring> utf8_to_wide(std::string_view utf8);

    /// @brief `std::wstring`（Windows 为 UTF-16LE，Linux 为 UCS-4）转 UTF-8 字符串。
    /// @param wide 输入 wchar 序列，**必须是合法编码**（不允许孤立代理项或超出
    ///             Unicode 标量值范围的码点），否则返回 `INVALID_ARGUMENT`。
    /// @note 内置纯算法实现，不依赖系统代码页；glibc iconv 曾接受超上限码点的
    ///       平台差异（commit 2d005cb）在内置路径下已消除。
    static core::StatusResult<std::string> wide_to_utf8(std::wstring_view wide);

    /// @brief 本地 ANSI 代码页字符串转 `std::wstring`。
    /// @note Windows 为 CP_ACP（系统区域，中文 Windows 通常是 GBK）；
    ///       POSIX 为当前 locale 的 codeset（未 setlocale 时为 ASCII）。
    ///       该编码跟随系统环境；codeset 不被系统支持（含纯内置构建下非
    ///       UTF-8 / ASCII / Latin-1 的 codeset）时返回 `UNIMPLEMENTED`。
    static core::StatusResult<std::wstring> local_to_wide(std::string_view local);

    /// @brief 本地 ANSI 代码页（CP_ACP）字符串转 UTF-8。等价于 `local_to_wide` + `wide_to_utf8`。
    static core::StatusResult<std::string> local_to_utf8(std::string_view local);

    /// @brief GBK（CP_936 口径）字符串转 UTF-8。
    /// @note 内置路径按 GB18030 处理（GBK 是其双字节子集），跨平台行为一致；
    ///       与 CP_936 的边角差异见类注释。
    static core::StatusResult<std::string> gbk_to_utf8(std::string_view gbk);

    /// @brief UTF-8 字符串转 GBK（CP_936 口径，内置按 GB18030 编码）。
    /// @note 含 GB18030 无法表示的码点时返回 `INVALID_ARGUMENT`。
    static core::StatusResult<std::string> utf8_to_gbk(std::string_view utf8);

    /// @brief GBK（CP_936 口径）字符串转 `std::wstring`。
    static core::StatusResult<std::wstring> gbk_to_wide(std::string_view gbk);

    /// @brief `std::wstring` 转 GBK（CP_936 口径，内置按 GB18030 编码）。
    static core::StatusResult<std::string> wide_to_gbk(std::wstring_view wide);

    /// @brief GB18030 字符串转 UTF-8。支持单字节（ASCII）、双字节与四字节序列。
    /// @note 语义同 python `gb18030` codec / WHATWG index-gb18030。
    static core::StatusResult<std::string> gb18030_to_utf8(std::string_view gb18030);

    /// @brief UTF-8 字符串转 GB18030。
    /// @note 任何合法 Unicode 标量值都可编码（增补平面走四字节序列）。
    static core::StatusResult<std::string> utf8_to_gb18030(std::string_view utf8);

    /// @brief GB18030 字符串转 `std::wstring`。
    static core::StatusResult<std::wstring> gb18030_to_wide(std::string_view gb18030);

    /// @brief `std::wstring` 转 GB18030。
    static core::StatusResult<std::string> wide_to_gb18030(std::wstring_view wide);

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
