//
// @brief ca::str 各字符串类型间的转换函数
// @author Canrad
// @date 2026/05/31
// @note
//   Utf8String ↔ CString : 直接拷贝字节
//   Utf8String ↔ WString : 逐码点编解码
//   WString 底层 wchar_t 编码是平台相关的:
//     Windows: UTF-16LE (需处理代理对)
//     Linux/macOS: UTF-32 (每个 wchar_t = 一个码点)
//

#ifndef LIBCA_STR_CONVERSION_HPP
#define LIBCA_STR_CONVERSION_HPP

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"
#include "cstring.hpp"
#include "wstring.hpp"

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

}  // namespace ca::str

#endif  // LIBCA_STR_CONVERSION_HPP
