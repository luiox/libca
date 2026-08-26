/// @file format.h
/// @author Canrad
/// @brief 字符串格式化和基础类型和字符串类型转换功能
/// @version 0.1
/// @date 2026-02-27
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_FORMAT_FORMAT_H
#define LIBCA_EM_FORMAT_FORMAT_H

#include <em_base/datatype.h>
#include <stdarg.h>

// 启用浮点支持（默认0，若需要则定义为1）
#ifndef FMT_ENABLE_FLOAT
#define FMT_ENABLE_FLOAT 0
#endif

// 启用宽度/精度支持（默认0，若需要则定义为1），即 %.Nf 和 %0Nd
#ifndef FMT_ENABLE_WIDTH_PRECISION
#define FMT_ENABLE_WIDTH_PRECISION 0
#endif

// 启用十六进制支持（默认0，若需要则定义为1），即 %x 和 %X
#ifndef FMT_ENABLE_HEX
#define FMT_ENABLE_HEX 0
#endif

// 固定N位小数的截断
#define FMT_FLOAT_MODE_FIXED 0
// 允许控制小数位的截断
#define FMT_FLOAT_MODE_SIMPLE 1
// 标准舍入
#define FMT_FLOAT_MODE_NORMAL 2

// 若启用浮点，还可选择浮点处理模式
#ifndef FMT_FLOAT_MODE
#define FMT_FLOAT_MODE FMT_FLOAT_MODE_FIXED
#endif

// 当浮点启用且模式为 FIXED 时，固定小数位数
#ifndef FMT_FIXED_DECIMALS
#define FMT_FIXED_DECIMALS 3U
#endif

// 默认精度（当未启用宽度精度时，浮点输出使用的默认小数位数）
#ifndef FMT_DEFAULT_PRECISION
#define FMT_DEFAULT_PRECISION 3U
#endif

#ifndef FMT_U32_TMP_BUF_SIZE
#define FMT_U32_TMP_BUF_SIZE 16U
#endif

#ifndef FMT_BASE_CONV_TMP_BUF_SIZE
#define FMT_BASE_CONV_TMP_BUF_SIZE 16U
#endif

#ifndef FMT_F64_TO_STR_TMP_BUF_SIZE
#define FMT_F64_TO_STR_TMP_BUF_SIZE 48U
#endif

/// @brief 格式化特性开关说明
///
/// - FMT_ENABLE_FLOAT=0 时，%f 视为不支持格式符，按 "%f" 原样风格输出
/// - FMT_ENABLE_WIDTH_PRECISION=0 时，不解析宽度/精度（如 %02d、%.2f）
/// - FMT_ENABLE_HEX=0 时，%x/%X 视为不支持格式符
///
/// 浮点模式（仅在 FMT_ENABLE_FLOAT=1 时有效）：
/// - FMT_FLOAT_MODE_FIXED：固定 FMT_FIXED_DECIMALS 位小数（截断）
/// - FMT_FLOAT_MODE_SIMPLE：按请求/默认精度截断
/// - FMT_FLOAT_MODE_NORMAL：按请求/默认精度舍入

/// @brief 将 u32 转为十进制字符串（不做边界检查）
///
/// 调用方需保证 @p buf 具备足够空间。
/// 最大长度为 10 个数字字符（不含结尾 '\0'，本函数也不会写入 '\0'）。
///
/// @param buf 输出缓冲区（不可为 NULL）
/// @param val 待转换的无符号 32 位整数
/// @return 实际写入的字符数；当 @p buf 为 NULL 时返回 0
usize u32_to_str(char* buf, u32 val);

/// @brief 将 u32 转为十进制字符串（安全版）
///
/// 最多写入 @p buf_len - 1 个字符，并保证输出以 '\0' 结尾。
/// 若发生截断，返回值为实际写入字符数（不包含 '\0'）。
///
/// @param buf 输出缓冲区
/// @param buf_len 输出缓冲区总长度（字节）
/// @param val 待转换的无符号 32 位整数
/// @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
usize u32_to_str_safe(char* buf, usize buf_len, u32 val);

/// @brief 将 f32 转为定点十进制字符串（截断小数，不做边界检查）
///
/// 输出格式示例：
/// - decimal_num=0: "123"
/// - decimal_num=2: "123.45"
///
/// 调用方需保证 @p buf 空间足够，本函数会写入结尾 '\0'。
///
/// @param buf 输出缓冲区（不可为 NULL）
/// @param val 待转换浮点值
/// @param decimal_num 小数位数
/// @return 实际写入字符数（不包含 '\0'）；当 @p buf 为 NULL 时返回 0
usize f32_to_str(char* buf, f32 val, u32 decimal_num);

/// @brief 将 f32 转为定点十进制字符串（安全版）
///
/// 最多写入 @p buf_len - 1 个字符，并保证输出以 '\0' 结尾。
/// 若发生截断，返回值为实际写入字符数（不包含 '\0'）。
///
/// @param buf 输出缓冲区
/// @param buf_len 输出缓冲区总长度（字节）
/// @param val 待转换浮点值
/// @param decimal_num 小数位数
/// @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
usize f32_to_str_safe(char* buf, usize buf_len, f32 val, u32 decimal_num);

/// @brief 将 f64 转为定点十进制字符串（截断小数，不做边界检查）
///
/// 调用方需保证 @p buf 空间足够，本函数会写入结尾 '\0'。
///
/// @param buf 输出缓冲区（不可为 NULL）
/// @param val 待转换浮点值
/// @param decimal_num 小数位数
/// @return 实际写入字符数（不包含 '\0'）；当 @p buf 为 NULL 时返回 0
usize f64_to_str(char* buf, f64 val, u32 decimal_num);

/// @brief 将 f64 转为定点十进制字符串（安全版）
///
/// 最多写入 @p buf_len - 1 个字符，并保证输出以 '\0' 结尾。
/// 若发生截断，返回值为实际写入字符数（不包含 '\0'）。
///
/// @param buf 输出缓冲区
/// @param buf_len 输出缓冲区总长度（字节）
/// @param val 待转换浮点值
/// @param decimal_num 小数位数
/// @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
usize f64_to_str_safe(char* buf, usize buf_len, f64 val, u32 decimal_num);

/// @brief 轻量级格式化（va_list 版本）
///
/// 基础支持格式：%d %u %s %%
///
/// 可选支持（由宏决定）：
/// - %f: 受 FMT_ENABLE_FLOAT 控制
/// - %x/%X: 受 FMT_ENABLE_HEX 控制
/// - %.Nf / %0Nd: 受 FMT_ENABLE_WIDTH_PRECISION 控制
///
/// 不支持的格式符按标准库常见习惯处理：输出 '%' + 该字符。
///
/// @param buf 输出缓冲区
/// @param buf_size 输出缓冲区总长度（字节）
/// @param fmt 格式字符串
/// @param args 可变参数列表
/// @return 标准 snprintf 语义：返回“本应写入字符数”（不包含 '\0'）；参数非法返回 0
i32 fmt_vsnprintf(char* buf, usize buf_size, const char* fmt, va_list args);

/// @brief 轻量级格式化（va_list 快速有界版本）
///
/// 与 fmt_vsnprintf 的格式能力一致，但返回值采用快速语义：
/// 发生截断时返回 @p buf_size - 1（实际写入字符数，不包含 '\0'）。
///
/// @param buf 输出缓冲区
/// @param buf_size 输出缓冲区总长度（字节）
/// @param fmt 格式字符串
/// @param args 可变参数列表
/// @return 实际写入字符数（不包含 '\0'）；参数非法返回 0
i32 fmt_vsnprintf_fast(char* buf, usize buf_size, const char* fmt, va_list args);

/// @brief 轻量级格式化（有界版本）
///
/// 最多写入 @p buf_size - 1 个字符，并保证结尾 '\0'。
///
/// @param buf 输出缓冲区
/// @param buf_size 输出缓冲区总长度（字节）
/// @param fmt 格式字符串
/// @param ... 可变参数
/// @return 标准 snprintf 语义：返回“本应写入字符数”（不包含 '\0'）；参数非法返回 0
i32 fmt_snprintf(char* buf, usize buf_size, const char* fmt, ...);

/// @brief 轻量级格式化（快速有界版本）
///
/// 最多写入 @p buf_size - 1 个字符，并保证结尾 '\0'。
///
/// 注意：该接口为非标准返回值语义，截断时返回 @p buf_size - 1，
/// 即“实际写入字符数”（不包含 '\0'），而不是“本应写入字符数”。
///
/// @param buf 输出缓冲区
/// @param buf_size 输出缓冲区总长度（字节）
/// @param fmt 格式字符串
/// @param ... 可变参数
/// @return 实际写入字符数（不包含 '\0'）；参数非法返回 0
i32 fmt_snprintf_fast(char* buf, usize buf_size, const char* fmt, ...);

/// @brief 轻量级格式化（无界版本）
///
/// 行为等价 sprintf，调用方需保证目标缓冲区空间足够。
///
/// @param buf 输出缓冲区
/// @param fmt 格式字符串
/// @param ... 可变参数
/// @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
i32 fmt_sprintf(char* buf, const char* fmt, ...);

#endif // !LIBCA_EM_FORMAT_FORMAT_H
