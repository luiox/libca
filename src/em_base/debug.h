//
// 用于调试时，串口打印有问题的代码
// 解决重定向fputc后，直接调用printf导致这些调试代码不容易删除的问题
// 使用方法： 1. 初始化串口相关内容，（这通常是使用HAL库或者是手动调用标准固件库实现）
//           2. 调用debug_init来初始化调试模块，（这用于确定信息输出到哪个串口）
//           3. 把DEBUG_INFO宏当做printf来打印信息，
//               （注意，这需要定义USE_DEBUG_MODE宏，这样的目的是，在非DEBUG模式下，可以把相关调试代码替换为空）
//
//

#ifndef LIBCA_EM_BASE_DEBUG_H
#define LIBCA_EM_BASE_DEBUG_H

#include "../em_base/base_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 设置默认打印缓冲区大小
#ifndef PRINT_BUFFER_SIZE
#    define PRINT_BUFFER_SIZE 256
#endif

// 定义换行符
#ifndef PRINT_NEWLINE
#    define PRINT_NEWLINE "\n"
#endif

void debug_init(void (*hw_puts_output)(const char* str));
void debug_puts(const char* str);
void debug_printf(const char* fmt, ...);

#if USE_DEBUG_MODE
#    define debug_print(fmt, ...) \
        debug_printf("[info][%s][%d]:" fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#    define debug_print(fmt, ...)
#endif   // USE_DEBUG_MODE

////////////////////////////////////////////////////////////////////////////////
// debug assert

#if USE_DEBUG_ASSERT
#    define debug_assert(expr)                                         \
        if (!(expr)) {                                                 \
            debug_print("assert failed: %s:%d\n", __FILE__, __LINE__); \
        }
#    define ASSERT_STATIC_(condition) typedef char c_assert_##__LINE__[(condition) ? 1 : -1]

#    define ASSERT_STATIC(condition)            \
        struct c_assert_##__LINE__              \
        {                                       \
            unsigned int : (condition) ? 1 : 0; \
        }

#else
#    define debug_assert(expr)
#    define ASSERT_STATIC_(condition)
#    define ASSERT_STATIC(condition)
#endif

#if USE_PARAM_CHECK
#    define param_check(expr)                                                   \
        if (!(expr)) {                                                          \
            debug_print("parameter check failed: %s:%d\n", __FILE__, __LINE__); \
        }
#else
#    define param_check(expr)
#endif

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_BASE_DEBUG_H
