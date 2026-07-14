/// @file simple_logger.h
/// @author canrad (1517807724@qq.com)
/// @brief 一个简单可以配置的的日志库
/// 是对em_log标准下的log语义的简单实现
/// @version 0.1
/// @date 2026-02-22
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_LOG_SIMPLE_LOGGER_H
#define LIBCA_EM_LOG_SIMPLE_LOGGER_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================
// 2. 用户配置区
// ==========================================

#ifndef LOG_MODULE_NAME
    #define LOG_MODULE_NAME ""
#endif

// 配置开关 (0/1)
#ifndef SLOG_ENABLE_TAG
    #define SLOG_ENABLE_TAG 1
#endif

#ifndef SLOG_DEBUG_SHOW_FILE_LINE
    #define SLOG_DEBUG_SHOW_FILE_LINE 1
#endif

#ifndef SLOG_BUFFER_SIZE
    #define SLOG_BUFFER_SIZE 128
#endif

#ifndef SLOG_LEVEL_BRIEF
    #define SLOG_LEVEL_BRIEF 0
#endif

#ifndef SLOG_ENABLE_RUNTIME_LEVEL
    #define SLOG_ENABLE_RUNTIME_LEVEL 0
#endif

#ifndef SLOG_USE_FAST_VSNPRINTF
    #define SLOG_USE_FAST_VSNPRINTF 0
#endif

// 静态过滤等级
#ifndef SLOG_COMPILE_LEVEL
    #define SLOG_COMPILE_LEVEL 4 // 4=DEBUG
#endif

// 锁是否启用，如果是RTOS建议实现锁
#ifndef SLOG_USER_LOCK
#define SLOG_USER_LOCK 0
#endif

// 锁的定义：
// 1) 当 SLOG_USER_LOCK=1 时，建议由用户在包含本头文件前自定义 SLOG_LOCK_ENTER/SLOG_LOCK_EXIT
// 2) 若未自定义，则默认降级为 no-op，保证可编译
// 3) 当 SLOG_USER_LOCK=0 时，固定为 no-op
#if SLOG_USER_LOCK
#ifndef SLOG_LOCK_ENTER
#define SLOG_LOCK_ENTER() ((void)0)
#endif
#ifndef SLOG_LOCK_EXIT
#define SLOG_LOCK_EXIT() ((void)0)
#endif
#else
#ifndef SLOG_LOCK_ENTER
#define SLOG_LOCK_ENTER() ((void)0)
#endif
#ifndef SLOG_LOCK_EXIT
#define SLOG_LOCK_EXIT() ((void)0)
#endif
#endif

// 日志等级
#define LOG_LEVEL_RAW   0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

// 输出函数类型定义
typedef void (*slog_output_fn_t)(const u8 *buf, usize len);

/// @brief 初始化 simple logger 的底层输出函数
///
/// @param out_fn 底层输出回调，传入 NULL 表示关闭输出
void slog_init(slog_output_fn_t out_fn);

/// @brief simple logger 的底层格式化输出函数
///
/// @param fmt printf 风格格式字符串
/// @param ... 格式参数
void _slog_printf(const char *fmt, ...);

#if SLOG_ENABLE_RUNTIME_LEVEL
/// @brief 设置运行时日志过滤等级
///
/// @param level 运行时等级，范围见 LOG_LEVEL_*
void slog_set_runtime_level(u8 level);

/// @brief 获取当前运行时日志过滤等级
///
/// @return u8 当前等级
u8 slog_get_runtime_level(void);

/// @brief 判断某等级日志在当前运行时配置下是否允许输出
///
/// @param level 待判断等级
/// @return bool true 表示允许输出
bool slog_should_log(u8 level);
#endif

// 等级字符串转换宏
#if SLOG_LEVEL_BRIEF
    #define _SLOG_LVL_STR_E "E"
    #define _SLOG_LVL_STR_W "W"
    #define _SLOG_LVL_STR_I "I"
    #define _SLOG_LVL_STR_D "D"
#else
    #define _SLOG_LVL_STR_E "ERROR"
    #define _SLOG_LVL_STR_W "WARN"
    #define _SLOG_LVL_STR_I "INFO"
    #define _SLOG_LVL_STR_D "DEBUG"
#endif

// TAG 拼接宏 (如果关闭则为空)
#if SLOG_ENABLE_TAG
    #define _SLOG_TAG_PART "[" LOG_MODULE_NAME "]"
#else
    #define _SLOG_TAG_PART
#endif

// 行号拼接宏 (辅助宏，用于将 __LINE__ 转成字符串)
#define _SLOG_STRINGIFY(x) #x
#define _SLOG_TOSTRING(x) _SLOG_STRINGIFY(x)

// 禁止直接调用 _slog_printf
// 最终生成格式: "[LEVEL][TAG] user_fmt\n"
#define _SLOG_CORE(level_str, fmt, ...) \
        _slog_printf("[%s]" _SLOG_TAG_PART " " fmt "\n", level_str, ##__VA_ARGS__)

// DEBUG 特殊宏: 需要额外的文件名行号
// 最终生成格式: "[DEBUG][TAG][file:line] user_fmt\n"
#if SLOG_DEBUG_SHOW_FILE_LINE
    #define _SLOG_CORE_DEBUG(fmt, ...) \
        _slog_printf("[%s]" _SLOG_TAG_PART "[%s:%d] " fmt "\n", \
                     _SLOG_LVL_STR_D, __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define _SLOG_CORE_DEBUG(fmt, ...) _SLOG_CORE(_SLOG_LVL_STR_D, fmt, ##__VA_ARGS__)
#endif

#if SLOG_ENABLE_RUNTIME_LEVEL
    #define _SLOG_RUNTIME_GUARD(level) if (slog_should_log((u8)(level)))
#else
    #define _SLOG_RUNTIME_GUARD(level) if (1)
#endif

#define log_raw(fmt, ...) _slog_printf(fmt, ##__VA_ARGS__)

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_ERROR
    #define log_error(fmt, ...) \
        do { _SLOG_RUNTIME_GUARD(LOG_LEVEL_ERROR) { _SLOG_CORE(_SLOG_LVL_STR_E, fmt, ##__VA_ARGS__); } } while (0)
#else
    #define log_error(fmt, ...) ((void)0)
#endif

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_WARN
    #define log_warn(fmt, ...)  \
        do { _SLOG_RUNTIME_GUARD(LOG_LEVEL_WARN) { _SLOG_CORE(_SLOG_LVL_STR_W, fmt, ##__VA_ARGS__); } } while (0)
#else
    #define log_warn(fmt, ...)  ((void)0)
#endif

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_INFO
    #define log_info(fmt, ...)  \
        do { _SLOG_RUNTIME_GUARD(LOG_LEVEL_INFO) { _SLOG_CORE(_SLOG_LVL_STR_I, fmt, ##__VA_ARGS__); } } while (0)
#else
    #define log_info(fmt, ...)  ((void)0)
#endif

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_DEBUG
    #define log_debug(fmt, ...) \
        do { _SLOG_RUNTIME_GUARD(LOG_LEVEL_DEBUG) { _SLOG_CORE_DEBUG(fmt, ##__VA_ARGS__); } } while (0)
#else
    #define log_debug(fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_LOG_SIMPLE_LOGGER_H
