/**
 * @file debug.h
 * @author canrad (1517807724@qq.com)
 * @brief 调试打印模块接口
 * 本模块用于在调试时将字符串输出到指定的硬件输出（例如串口）。
 * 提供初始化、直接输出和格式化输出接口，以及调试辅助宏（debug_print、debug_assert、param_check）。
 *
 * 使用方法：
 * 1. 初始化串口或其他输出设备（例如使用 HAL 库或裸机驱动）。
 * 2. 调用 debug_init 注册输出回调函数。
 * 3. 使用 debug_print 输出调试信息。
 *
 * 控制宏如下，定义为0时候可以关闭，定义为1时候打开，默认全部打开。
 * USE_DEBUG_MODE、USE_DEBUG_ASSERT、USE_PARAM_CHECK
 * 自定义，配置文件宏：USE_CUSTOM_DEBUG_CONFIG，这个宏默认关闭
 *
 * @version 0.1
 * @date 2026-01-12
 * 2026-01-31 第一次明确debug的接口
 *
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_BASE_DEBUG_H
#define LIBCA_EM_BASE_DEBUG_H

#ifdef USE_CUSTOM_DEBUG_CONFIG
#include USE_CUSTOM_DEBUG_CONFIG
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 如果没有定义，那么提供默认值
#ifndef USE_DEBUG_MODE
// 使用串口打印信息，debug模块需要定义这个宏为1
#define USE_DEBUG_MODE 1
#endif

#ifndef USE_DEBUG_ASSERT
// 使用调试断言，需要定义这个宏为1
#define USE_DEBUG_ASSERT 1
#endif

#ifndef USE_PARAM_CHECK
// 使用参数检查，需要定义这个宏为1
#define USE_PARAM_CHECK 1
#endif

// 设置默认打印缓冲区大小
#ifndef PRINT_BUFFER_SIZE
#    define PRINT_BUFFER_SIZE 256
#endif

// 定义换行符
#ifndef PRINT_NEWLINE
#    define PRINT_NEWLINE "\n"
#endif

/**
 * @brief 初始化调试模块
 *
 * 注册底层字符串输出回调函数，后续的 debug_puts/debug_printf
 * 会通过该回调输出字符串。
 *
 * @param hw_puts_output 输出回调函数，参数为以 NUL 结尾的字符串；
 *                        传入 NULL 将取消注册。
 */
void debug_init(void (*hw_puts_output)(const char* str));

/**
 * @brief 直接输出字符串到调试接口
 *
 * 将字符串传递到注册的输出回调。字符串应以 NUL 结尾。
 *
 * @param str 要输出的字符串
 */
void debug_puts(const char* str);

/**
 * @brief 格式化输出到调试接口
 *
 * 使用内部缓冲区进行格式化（大小由 PRINT_BUFFER_SIZE 定义），
 * 然后调用输出回调发送结果。
 *
 * @param fmt printf 风格的格式字符串
 * @param ... 格式化参数
 */
void debug_printf(const char* fmt, ...);

/**
 * @brief 调试输出宏（仅在 USE_DEBUG_MODE 定义时生效）
 *
 * 该宏在编译选项启用时，会在输出中追加文件名和行号以便定位。
 */
#if USE_DEBUG_MODE
#    define debug_print(fmt, ...) \
        debug_printf("[%s:%d]:" fmt PRINT_NEWLINE, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#    define debug_print(fmt, ...)
#endif   // USE_DEBUG_MODE

////////////////////////////////////////////////////////////////////////////////
// debug 断言

#if USE_DEBUG_ASSERT
/**
 * @brief 调试断言（在 USE_DEBUG_ASSERT 启用时有效）
 *
 * 若条件不满足，会通过 debug_print 打印断言失败信息（文件及行号）。
 */
#    define debug_assert(expr)                                         \
        if (!(expr)) {                                                 \
            debug_print("assert failure: %s:%d" PRINT_NEWLINE, __FILE__, __LINE__); \
        }
#else
#endif

#if USE_PARAM_CHECK
/**
 * @brief 参数检查宏（在 USE_PARAM_CHECK 启用时有效）
 *
 * 若条件不满足，会通过 debug_print 打印参数检查失败信息（文件及行号）。
 */
#    define param_check(expr)                                                   \
        if (!(expr)) {                                                          \
            debug_print("param check failure: %s:%d" PRINT_NEWLINE, __FILE__, __LINE__); \
        }
#else
#    define param_check(expr)
#endif

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_BASE_DEBUG_H
