#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

// 日志级别定义
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

// 默认日志级别为DEBUG
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

// 日志输出目标
#define LOG_TARGET_CONSOLE 0
#define LOG_TARGET_FILE    1

// 前置声明
struct log_config {
    int level;           // 日志级别
    int target;          // 输出目标
    FILE *file;          // 日志文件指针
    char file_path[256]; // 日志文件路径
};

// 函数声明
void log_init(int level, int target, const char *file_path);
void log_destroy(void);
void log_print(int level, const char *tag, const char *fmt, ...);
void log_vprint(int level, const char *tag, const char *fmt, va_list args);

// 根据日志级别定义宏，方便使用
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(tag, fmt, ...) log_print(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(tag, fmt, ...) log_print(LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(tag, fmt, ...) log_print(LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(tag, fmt, ...) log_print(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(tag, fmt, ...) ((void)0)
#endif

#endif // LOG_H