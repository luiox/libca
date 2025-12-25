//
// 用于调试时，串口打印有问题的代码
// 解决重定向fputc后，直接调用printf导致这些调试代码不容易删除的问题
// 使用方法： 1. 初始化串口相关内容，（这通常是使用HAL库或者是手动调用标准固件库实现）
//           2. 调用debug_init来初始化调试模块，（这用于确定信息输出到哪个串口）
//           3. 把DEBUG_INFO宏当做printf来打印信息，
//               （注意，这需要定义USE_DEBUG_MODE宏，这样的目的是，在非DEBUG模式下，可以把相关调试代码替换为空）
//
//

#ifndef MYLIB_UTILITY_DEBUG_H
#define MYLIB_UTILITY_DEBUG_H

#include "../em_base/base_config.h"
#include "datatype.h"
#include <stdarg.h>

#define PRI_LEVEL_DEBUG 0
#define PRI_LEVEL_INFO 1
#define PRI_LEVEL_WARN 2
#define PRI_LEVEL_ERROR 3
#define PRI_LEVEL_FATAL 4

// 设置默认打印级别
#ifndef PRINT_LEVEL_DEFAULT
#define PRINT_LEVEL_DEFAULT PRI_LEVEL_DEBUG
#endif

// 设置默认打印缓冲区大小
#ifndef PRINT_BUFFER_SIZE
#define PRINT_BUFFER_SIZE 256
#endif

// 定义换行符
#ifndef PRINT_NEWLINE
#define PRINT_NEWLINE "\n"
#endif

void ca_puts(const char* str);
void ca_set_print_level(i8 level);
void ca_printf(i8 level, const char* fmt, ...);
void ca_println(i8 level, const char* str);
void ca_dprintf(const char* fmt, ...);


////////////////////////////////////////////////////////////////////////////////
// debug print

void debug_print(const char* fmt, ...);

#ifdef USE_DEBUG_MODE
#    define DEBUG_INFO(fmt, ...) \
        ca_dprintf("[info][%s][%d]:" fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#    define DEBUG_INFO(fmt, ...)
#endif   // USE_DEBUG_MODE

////////////////////////////////////////////////////////////////////////////////
// debug assert

#if USE_DEBUG_ASSERT
#    define MYLIB_DEBUG_ASSERT(expr)                                   \
        if (!(expr)) {                                                 \
            ca_dprintf("assert failed: %s:%d\n", __FILE__, __LINE__); \
        }
#else
#    define MYLIB_DEBUG_ASSERT(expr)
#endif

#if USE_PARAM_CHECK

#    define CA_PARAM_CHECK(expr)                                            \
        if (!(expr)) {                                                      \
            ca_dprintf("param check failed: %s:%d\n", __FILE__, __LINE__); \
        }

#else

#    define CA_PARAM_CHECK(expr) \
        {}

#endif

////////////////////////////////////////////////////////////////////////////////
// debug assert

#if USE_DEBUG_ASSERT
#    define LIBCA_DEBUG_ASSERT(expr)                                   \
        if (!(expr)) {                                                 \
            ca_dprintf("assert failed: %s:%d\n", __FILE__, __LINE__); \
        }
#else
#    define LIBCA_DEBUG_ASSERT(expr) \
        {}
#endif

// #define C_ASSERT_STATIC(condition) \
//     typedef char c_assert_##__LINE__[(condition) ? 1 : -1]

// #define C_ASSERT_STATIC(condition) \
//     struct c_assert_##__LINE__ { unsigned int : (condition) ? 1 : 0; }

#ifdef __cplusplus

#    ifdef NDEBUG

#        define ca_assert(expr) static_cast<void>(0)

#    else

#        define ca_assert(expr) \
            static_cast<void>(expr) ? 0 : (::ca::assertion_failed(#expr, __FILE__, __LINE__), 0)

#    endif   // NDEBUG


#endif   // __cplusplus

#ifndef NDEBUG

#    define ca_assert(expr) ((void)0)
#else

#    define ca_assert(expr) ((void)0)

#endif   // NDEBUG

#endif   // !MYLIB_BASE_DEBUG_H
