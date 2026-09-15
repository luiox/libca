//
// @brief 字符编码内置转换核心（Tier 1 纯算法实现，零平台/外部依赖）。
// @author Canrad
// @date 2026/09/15
//
// 本头文件是 str 模块内部实现，不对外导出；对外一律走 CharsetConverter 门面。
// 三级查找结构（见 doc/加密与编码内置化方案.md §一）：
//   内置实现（Tier 1 纯算法 / Tier 2 表驱动）→ iconv 回落（Tier 3）→ UNSUPPORTED。
// 所有函数错误语义统一：非法输入序列返回 INVALID_ARGUMENT，绝不静默替换字节。
//

#pragma once

#include "libca/core/status.hpp"

#include <string>
#include <string_view>

namespace ca::str::detail {

// ============================================================================
// UTF 家族（Tier 1）：UTF-8 ↔ wchar（Windows UTF-16LE / POSIX UCS-4）。
// 复用 conversion.hpp / utf8_util.hpp 的纯算法原语，不依赖系统代码页。
// ============================================================================

// UTF-8 → std::wstring。严格校验 UTF-8（含续字节与码点合法性），非法返回 INVALID_ARGUMENT。
core::StatusResult<std::wstring> utf8_to_wide(std::string_view utf8);

// std::wstring → UTF-8。拒绝孤立代理项（Windows）与超出 Unicode 标量值范围
// 的码点（POSIX，>U+10FFFF 一律报错——glibc iconv 接受超上限码点的历史差异就此消除）。
core::StatusResult<std::string> wide_to_utf8(std::wstring_view wide);

// ============================================================================
// Latin-1（ISO-8859-1，Tier 1 直映射）：字节 0x00-0xFF ↔ 码点 U+0000-U+00FF。
// ============================================================================

core::StatusResult<std::string> latin1_to_utf8(std::string_view latin1);

// UTF-8 → Latin-1。含 >U+00FF 码点时返回 INVALID_ARGUMENT（不可表示，不替换）。
core::StatusResult<std::string> utf8_to_latin1(std::string_view utf8);

core::StatusResult<std::wstring> latin1_to_wide(std::string_view latin1);

// std::wstring → Latin-1。含 >U+00FF 码点时返回 INVALID_ARGUMENT。
core::StatusResult<std::string> wide_to_latin1(std::wstring_view wide);

// ============================================================================
// Windows-1252（Tier 1）：= Latin-1 + 0x80-0x9F 区段的 WHATWG index-windows-1252
// 差异表（27 项）。0x81/0x8D/0x8F/0x90/0x9D 这 5 个 WHATWG 未定义字节按 Latin-1
// 恒等映射为对应 C1 control（全映射双射，同 Windows CP_1252 best-fit；
// WHATWG 解码器与 python cp1252 codec 对这 5 字节报错，此处有意放宽，见设计文档）。
// ============================================================================

core::StatusResult<std::string> cp1252_to_utf8(std::string_view cp1252);

// UTF-8 → Windows-1252。含既非 Latin-1 范围、也非 27 项差异码点时返回 INVALID_ARGUMENT。
core::StatusResult<std::string> utf8_to_cp1252(std::string_view utf8);

core::StatusResult<std::wstring> cp1252_to_wide(std::string_view cp1252);

// std::wstring → Windows-1252。不可表示时返回 INVALID_ARGUMENT。
core::StatusResult<std::string> wide_to_cp1252(std::wstring_view wide);

}  // namespace ca::str::detail
