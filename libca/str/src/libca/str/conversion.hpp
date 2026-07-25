/// @file conversion.hpp
/// @brief ca::str 各字符串类型间的转换：Utf8String ↔ CString（拷字节）/ WString（逐码点编解码）。
/// @author Canrad
/// @date 2026/05/31
/// @note WString 底层 wchar_t 编码平台相关：Windows = UTF-16LE（含代理对），Linux/macOS = UTF-32。
///       非法 UTF-8 / 非法码点的转换会抛 std::runtime_error。

#pragma once

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"
#include "cstring.hpp"
#include "wstring.hpp"
#include <stdexcept>

namespace ca::str {

// ============================================================================
// Utf8String  ↔ CString
// ============================================================================

// Utf8String → CString：直接拷贝字节
// Utf8String 内部存储的是 UTF-8 编码的 u8[]，按字节拷贝为 char[]
CString toCString(const Utf8StringRef& str);

// CString → Utf8String：将 char 字节作为 UTF-8 解析
// 若 CString 中的 char 数据不是合法 UTF-8，抛出 std::runtime_error
Utf8String toUtf8String(const CStringRef& str);


// ============================================================================
// Utf8String  ↔ WString
// ============================================================================

// Utf8String → WString：每个 UTF-8 码点编码为 wchar_t
// 平台相关: Windows 上输出 UTF-16LE, Linux/macOS 上输出 UTF-32
WString toWString(const Utf8StringRef& str);

// WString → Utf8String：每个 wchar_t 解码为 UTF-8 码点
// 若遇到非法码点则抛出 std::runtime_error
Utf8String toUtf8String(const WStringRef& str);


// ============================================================================
// CString ↔ WString
// ============================================================================

// CString → WString：先将 C 字符串作为 UTF-8 解码，再编码为 wchar_t
WString toWString(const CStringRef& str);

// WString → CString：先将 wchar_t 解码为码点，再以 UTF-8 编码为 char
// 注意：若 CString 仅需 ASCII/Latin-1 可直接逐字节截断，本函数做完整转换
CString toCString(const WStringRef& str);


// ============================================================================
// UTF-8 ↔ raw UTF-16 (u16) 工具
// ============================================================================

// 计算 UTF-8 转换成 UTF-16 后的 u16 单元数量（含代理对）
// 返回 0 表示输入非法
usize utf8ToUtf16Length(const u8* utf8, usize byteLength) noexcept;

// UTF-8 → UTF-16 (u16 数组)
// utf16 缓冲区应能容纳至少 utf8ToUtf16Length 个 u16
// 返回写入的 u16 个数，0 表示输入非法
usize utf8ToUtf16(const u8* utf8, usize byteLength, u16* utf16) noexcept;

// 计算 UTF-16 转换成 UTF-8 后的字节数
// 返回 0 表示输入非法
usize utf16ToUtf8Length(const u16* utf16, usize unitCount) noexcept;

// UTF-16 → UTF-8
// utf8 缓冲区应能容纳至少 utf16ToUtf8Length 个 u8
// 返回写入的 u8 个数，0 表示输入非法
usize utf16ToUtf8(const u16* utf16, usize unitCount, u8* utf8) noexcept;


// ============================================================================
// UTF-8 ↔ UTF-32 (u32 码点数组) 工具
// ============================================================================
// 跨平台：直接以 u32 承载码点，不依赖 wchar_t 的平台宽度（Windows 上 wchar_t 为
// UTF-16，无法承载单个 astral 码点）。非法 UTF-8 / 非法码点（代理项、>U+10FFFF）返回 0。

// 计算 UTF-8 转成 UTF-32 后的码点（u32 单元）个数。返回 0 表示输入非法。
usize utf8_to_utf32_length(const u8* utf8, usize byteLength) noexcept;

// UTF-8 → UTF-32：utf32 缓冲区应能容纳至少 utf8_to_utf32_length 个 u32。
// 返回写入的 u32 个数，0 表示输入非法。
usize utf8_to_utf32(const u8* utf8, usize byteLength, u32* utf32) noexcept;

// 计算 UTF-32 转成 UTF-8 后的字节数。任一码点非法（代理项 / >U+10FFFF）返回 0。
usize utf32_to_utf8_length(const u32* utf32, usize count) noexcept;

// UTF-32 → UTF-8：utf8 缓冲区应能容纳至少 utf32_to_utf8_length 个 u8。
// 返回写入的 u8 个数，0 表示输入含非法码点。
usize utf32_to_utf8(const u32* utf32, usize count, u8* utf8) noexcept;


// ============================================================================
// Latin-1 (ISO-8859-1) ↔ UTF-8 工具
// ============================================================================
// Latin-1 每字节 0x00-0xFF 对应码点 U+0000..U+00FF。跨平台、无外部依赖。

// 计算 Latin-1 转成 UTF-8 后的字节数（0x00-0x7F 占 1 字节，0x80-0xFF 占 2 字节）。
usize latin1_to_utf8_length(const u8* latin1, usize length) noexcept;

// Latin-1 → UTF-8：utf8 缓冲区应能容纳至少 latin1_to_utf8_length 个 u8。
// 恒成功（每个 Latin-1 字节都是合法码点），返回写入的 u8 个数。
usize latin1_to_utf8(const u8* latin1, usize length, u8* utf8) noexcept;

// 计算 UTF-8 转成 Latin-1 后的字节数（= 码点数）。
// 输入非法 UTF-8，或含 >U+00FF 的码点（Latin-1 不可表示）时返回哨兵。
// 用返回值区分 0（空输入合法）与不可表示：见 UTF8_TO_LATIN1_INVALID。
static constexpr usize UTF8_TO_LATIN1_INVALID = usize(-1);
usize utf8_to_latin1_length(const u8* utf8, usize byteLength) noexcept;

// UTF-8 → Latin-1：latin1 缓冲区应能容纳至少 utf8_to_latin1_length 个 u8。
// 返回写入的 u8 个数；输入非法或含 >U+00FF 码点时返回 UTF8_TO_LATIN1_INVALID。
usize utf8_to_latin1(const u8* utf8, usize byteLength, u8* latin1) noexcept;

}  // namespace ca::str
