#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 日志配置结构体
static struct log_config g_log_config = {
    .level = LOG_LEVEL_DEBUG,
    .target = LOG_TARGET_CONSOLE,
    .file = NULL
};

// 日志级别字符串映射
static const char *level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

/**
 * 初始化日志系统
 * @param level 日志级别
 * @param target 日志输出目标
 * @param file_path 日志文件路径（当target为LOG_TARGET_FILE时有效）
 */
void log_init(int level, int target, const char *file_path) {
    g_log_config.level = level;
    g_log_config.target = target;
    
    if (target == LOG_TARGET_FILE && file_path != NULL) {
        strncpy(g_log_config.file_path, file_path, sizeof(g_log_config.file_path) - 1);
        g_log_config.file_path[sizeof(g_log_config.file_path) - 1] = '\0';
        g_log_config.file = fopen(file_path, "a");
    }
}

/**
 * 销毁日志系统，释放资源
 */
void log_destroy(void) {
    if (g_log_config.file != NULL) {
        fclose(g_log_config.file);
        g_log_config.file = NULL;
    }
}

/**
 * 格式化输出日志
 * @param level 日志级别
 * @param tag 日志标签
 * @param fmt 格式化字符串
 * @param args 参数列表
 */
void log_vprint(int level, const char *tag, const char *fmt, va_list args) {
    // 检查日志级别
    if (level < g_log_config.level) {
        return;
    }

    // 获取当前时间
    time_t now;
    time(&now);
    struct tm *local_time = localtime(&now);
    
    // 选择输出目标
    FILE *output = (g_log_config.target == LOG_TARGET_CONSOLE) ? stdout : g_log_config.file;
    if (g_log_config.target == LOG_TARGET_FILE && g_log_config.file == NULL) {
        output = stdout; // 如果文件不可用，则输出到控制台
    }
    
    // 输出日志信息
    fprintf(output, "[%04d-%02d-%02d %02d:%02d:%02d][%s][%s] ",
            local_time->tm_year + 1900,
            local_time->tm_mon + 1,
            local_time->tm_mday,
            local_time->tm_hour,
            local_time->tm_min,
            local_time->tm_sec,
            level_strings[level],
            tag);
    
    vfprintf(output, fmt, args);
    fprintf(output, "\n");
    fflush(output);
}

/**
 * 输出日志
 * @param level 日志级别
 * @param tag 日志标签
 * @param fmt 格式化字符串
 */
void log_print(int level, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vprint(level, tag, fmt, args);
    va_end(args);
}
