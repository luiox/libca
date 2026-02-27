#ifndef FORMAT_H
#define FORMAT_H

#include "datatype.h"
#include <stdarg.h>

// 无符号整数转字符串
usize u32_to_str(char* buf, u32 val);

// 浮点数转字符串
void f32_to_str(char* buf, float val, u32 decimal_num);

// 轻量格式化：支持 %d %u %f %x %X %s %% ，支持 %.Nf 与 %0Nd
i32 fmt_vsnprintf(char* buf, usize buf_size, const char* fmt, va_list args);

// 轻量格式化：不带长度限制（行为等价 sprintf，需调用方保证缓冲区足够）
i32 fmt_sprintf(char* buf, const char* fmt, ...);

#endif // !FORMAT_H
