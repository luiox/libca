//
// @brief 字符编码内置实现的内部共享助手（str 模块私有，不对外导出）。
// @author Canrad
// @date 2026/09/15
//
// 供 detail/charset_builtin.cpp（Tier 1 纯算法）与 detail/charset_gb18030.cpp
// （Tier 2 表驱动）共用：统一错误构造、严格 UTF-8 解码游标、wchar 码点互转。
//

#pragma once

#include "libca/core/datatype.hpp"
#include "libca/core/status.hpp"

#include <string>
#include <string_view>

namespace ca::str::detail {

// 统一错误构造：非法 / 残缺序列（带编码名与出错字节位置）。
core::Status charset_invalid_sequence(const char* charset, usize pos);

// 统一错误构造：码点超出目标编码可表示范围（不静默替换字节）。
core::Status charset_unrepresentable(const char* charset, u32 cp);

// 解码 UTF-8 下一个序列：pos 前进、cp_out 输出码点；非法 / 残缺返回 false（pos 不动）。
// 含码点合法性校验：拒绝代理项（含 CESU-8 式 ED A0 80 编码）；
// >U+10FFFF 无法由合法 UTF-8 表达，utf8_code_point_bytes 已排除此类首字节。
bool utf8_next_code_point(const u8* data, usize size, usize& pos, u32& cp_out);

// std::wstring → 码点序列（严格校验：孤立 / 无配对代理项与 >U+10FFFF 一律报错）。
core::StatusResult<std::u32string> wide_to_code_points(std::wstring_view wide);

// 追加一个码点到 std::wstring（Windows 拆代理对，POSIX 直拷）。
// 调用方保证 cp 是合法标量值（非代理项、≤U+10FFFF）。
void wide_push_code_point(std::wstring& out, u32 cp);

}  // namespace ca::str::detail
