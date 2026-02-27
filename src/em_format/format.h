/**
 * @file format.h
 * @author canrad (1517807724@qq.com)
 * @brief 字符串格式化和基础类型和字符串类型转换功能
 * @version 0.1
 * @date 2026-02-27
 */
#ifndef LIBCA_EM_FORMAT_FORMAT_H
#define LIBCA_EM_FORMAT_FORMAT_H

#include "../em_base/datatype.h"
#include <stdarg.h>

#ifndef FMT_ENABLE_FLOAT
#define FMT_ENABLE_FLOAT 0
#endif

#ifndef FMT_ENABLE_WIDTH_PRECISION
#define FMT_ENABLE_WIDTH_PRECISION 0
#endif

#ifndef FMT_ENABLE_HEX
#define FMT_ENABLE_HEX 0
#endif

#define FMT_FLOAT_MODE_FIXED 0
#define FMT_FLOAT_MODE_SIMPLE 1
#define FMT_FLOAT_MODE_NORMAL 2

#ifndef FMT_FLOAT_MODE
#define FMT_FLOAT_MODE FMT_FLOAT_MODE_FIXED
#endif

#ifndef FMT_FIXED_DECIMALS
#define FMT_FIXED_DECIMALS 3U
#endif

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

usize u32_to_str(char* buf, u32 val);
usize u32_to_str_safe(char* buf, usize buf_len, u32 val);

usize f32_to_str(char* buf, f32 val, u32 decimal_num);
usize f32_to_str_safe(char* buf, usize buf_len, f32 val, u32 decimal_num);

usize f64_to_str(char* buf, f64 val, u32 decimal_num);
usize f64_to_str_safe(char* buf, usize buf_len, f64 val, u32 decimal_num);

i32 fmt_vsnprintf(char* buf, usize buf_size, const char* fmt, va_list args);
i32 fmt_vsnprintf_fast(char* buf, usize buf_size, const char* fmt, va_list args);
i32 fmt_snprintf(char* buf, usize buf_size, const char* fmt, ...);
i32 fmt_snprintf_fast(char* buf, usize buf_size, const char* fmt, ...);
i32 fmt_sprintf(char* buf, const char* fmt, ...);

#endif // !LIBCA_EM_FORMAT_FORMAT_H
