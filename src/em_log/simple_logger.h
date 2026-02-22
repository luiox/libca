#ifndef LIBCA_EM_LOG_SIMPLE_LOGGER_H
#define LIBCA_EM_LOG_SIMPLE_LOGGER_H

#include "../em_base/datatype.h"

// ==========================================
// 1. 后端接口定义
// ==========================================
typedef void (*slog_output_fn_t)(const u8 *buf, usize len);
void slog_init(slog_output_fn_t out_fn);

// 底层实现：只需要一个格式化输出函数
void _slog_printf(const char *fmt, ...);

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

// 静态过滤等级
#ifndef SLOG_COMPILE_LEVEL
    #define SLOG_COMPILE_LEVEL 4 // 4=DEBUG
#endif

// ==========================================
// 3. 内部宏拼接工具
// ==========================================

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

// ==========================================
// 4. 核心日志宏
// ==========================================

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

// ==========================================
// 5. 用户 API (带静态过滤)
// ==========================================

#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_ERROR
    #define log_error(fmt, ...) _SLOG_CORE(_SLOG_LVL_STR_E, fmt, ##__VA_ARGS__)
#else
    #define log_error(fmt, ...) ((void)0)
#endif

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_WARN
    #define log_warn(fmt, ...)  _SLOG_CORE(_SLOG_LVL_STR_W, fmt, ##__VA_ARGS__)
#else
    #define log_warn(fmt, ...)  ((void)0)
#endif

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_INFO
    #define log_info(fmt, ...)  _SLOG_CORE(_SLOG_LVL_STR_I, fmt, ##__VA_ARGS__)
#else
    #define log_info(fmt, ...)  ((void)0)
#endif

#if SLOG_COMPILE_LEVEL >= LOG_LEVEL_DEBUG
    #define log_debug(fmt, ...) _SLOG_CORE_DEBUG(fmt, ##__VA_ARGS__)
#else
    #define log_debug(fmt, ...) ((void)0)
#endif

#endif // !LIBCA_EM_LOG_SIMPLE_LOGGER_H
