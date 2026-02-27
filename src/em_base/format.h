/**
 * @file format.h
 * @author canrad (1517807724@qq.com)
 * @brief 字符串格式化和基础类型和字符串类型转换功能
 * @version 0.1
 * @date 2026-02-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_BASE_FORMAT_H
#define LIBCA_EM_BASE_FORMAT_H

#include "datatype.h"
#include <stdarg.h>

#ifndef FMT_U32_TMP_BUF_SIZE
#define FMT_U32_TMP_BUF_SIZE 16U
#endif

#ifndef FMT_BASE_CONV_TMP_BUF_SIZE
#define FMT_BASE_CONV_TMP_BUF_SIZE 16U
#endif

#ifndef FMT_F64_TO_STR_TMP_BUF_SIZE
#define FMT_F64_TO_STR_TMP_BUF_SIZE 48U
#endif

/**
 * @brief 将 u32 转为十进制字符串（不做边界检查）
 *
 * 调用方需保证 @p buf 具备足够空间。
 * 最大长度为 10 个数字字符（不含结尾 '\0'，本函数也不会写入 '\0'）。
 *
 * @param buf 输出缓冲区（不可为 NULL）
 * @param val 待转换的无符号 32 位整数
 * @return 实际写入的字符数；当 @p buf 为 NULL 时返回 0
 */
usize u32_to_str(char* buf, u32 val);

/**
 * @brief 将 u32 转为十进制字符串（安全版）
 *
 * 最多写入 @p buf_len - 1 个字符，并保证输出以 '\0' 结尾。
 * 若发生截断，返回值为实际写入字符数（不包含 '\0'）。
 *
 * @param buf 输出缓冲区
 * @param buf_len 输出缓冲区总长度（字节）
 * @param val 待转换的无符号 32 位整数
 * @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
 */
usize u32_to_str_safe(char* buf, usize buf_len, u32 val);

/**
 * @brief 将 f32 转为定点十进制字符串（截断小数，不做边界检查）
 *
 * 输出格式示例：
 * - decimal_num=0: "123"
 * - decimal_num=2: "123.45"
 *
 * 调用方需保证 @p buf 空间足够，本函数会写入结尾 '\0'。
 *
 * @param buf 输出缓冲区（不可为 NULL）
 * @param val 待转换浮点值
 * @param decimal_num 小数位数
 * @return 实际写入字符数（不包含 '\0'）；当 @p buf 为 NULL 时返回 0
 */
usize f32_to_str(char* buf, f32 val, u32 decimal_num);

/**
 * @brief 将 f32 转为定点十进制字符串（安全版）
 *
 * 最多写入 @p buf_len - 1 个字符，并保证输出以 '\0' 结尾。
 * 若发生截断，返回值为实际写入字符数（不包含 '\0'）。
 *
 * @param buf 输出缓冲区
 * @param buf_len 输出缓冲区总长度（字节）
 * @param val 待转换浮点值
 * @param decimal_num 小数位数
 * @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
 */
usize f32_to_str_safe(char* buf, usize buf_len, f32 val, u32 decimal_num);

/**
 * @brief 将 f64 转为定点十进制字符串（截断小数，不做边界检查）
 *
 * 调用方需保证 @p buf 空间足够，本函数会写入结尾 '\0'。
 *
 * @param buf 输出缓冲区（不可为 NULL）
 * @param val 待转换浮点值
 * @param decimal_num 小数位数
 * @return 实际写入字符数（不包含 '\0'）；当 @p buf 为 NULL 时返回 0
 */
usize f64_to_str(char* buf, f64 val, u32 decimal_num);

/**
 * @brief 将 f64 转为定点十进制字符串（安全版）
 *
 * 最多写入 @p buf_len - 1 个字符，并保证输出以 '\0' 结尾。
 * 若发生截断，返回值为实际写入字符数（不包含 '\0'）。
 *
 * @param buf 输出缓冲区
 * @param buf_len 输出缓冲区总长度（字节）
 * @param val 待转换浮点值
 * @param decimal_num 小数位数
 * @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
 */
usize f64_to_str_safe(char* buf, usize buf_len, f64 val, u32 decimal_num);

/**
 * @brief 轻量级格式化（va_list 版本）
 *
 * 支持的格式：%d %u %f %x %X %s %%
 * 额外支持：%.Nf 与 %0Nd
 *
 * @param buf 输出缓冲区
 * @param buf_size 输出缓冲区总长度（字节）
 * @param fmt 格式字符串
 * @param args 可变参数列表
 * @return 实际写入字符数（不包含 '\0'）；发生截断时返回 @p buf_size - 1；参数非法返回 0
 */
i32 fmt_vsnprintf(char* buf, usize buf_size, const char* fmt, va_list args);

/**
 * @brief 轻量级格式化（有界版本）
 *
 * 最多写入 @p buf_size - 1 个字符，并保证结尾 '\0'。
 *
 * 注意：本实现与标准 snprintf 的返回值语义不同。
 * 标准 snprintf 在截断时返回“本应写入长度”，而本实现在截断时返回 @p buf_size - 1。
 *
 * @param buf 输出缓冲区
 * @param buf_size 输出缓冲区总长度（字节）
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 实际写入字符数（不包含 '\0'）；发生截断时返回 @p buf_size - 1；参数非法返回 0
 */
i32 fmt_snprintf(char* buf, usize buf_size, const char* fmt, ...);

/**
 * @brief 轻量级格式化（无界版本）
 *
 * 行为等价 sprintf，调用方需保证目标缓冲区空间足够。
 *
 * @param buf 输出缓冲区
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 实际写入字符数（不包含 '\0'）；当参数非法时返回 0
 */
i32 fmt_sprintf(char* buf, const char* fmt, ...);

#endif // !LIBCA_EM_BASE_FORMAT_H
