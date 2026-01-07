/**
 * @file log.h
 * @author canrad (1517807724@qq.com)
 * @brief 实现一个异步的日志，仅适用于单核MCU
 * 依赖soft_timer的时间获取接口以及可选的async异步工作队列
 * 实现多后端，tag可以过滤，解耦耗时IO输出到异步实现
 * @version 0.1
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_LOG_LOG_H
#define LIBCA_EM_LOG_LOG_H

#include "../em_base/datatype.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log levels
 */
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
} log_level_t;

/**
 * @brief Log record structure passed to backends
 */
typedef struct {
    log_level_t level;
    u32 time_sec;
    u16 time_ms;
    u16 time_us;
    // tag，必须是静态字符串
    const char* tag;
    // payload，格式化后的日志消息
    const char* payload;
    // payload长度
    usize payload_len;
} log_record_t;

typedef struct log_backend log_backend_t;

/**
 * @brief Log backend interface
 */
struct log_backend {
    const char* name;
    // 该backend的最低日志级别
    log_level_t min_level;
    // 是否启用该backend
    bool enabled;
    // 是否支持ANSI颜色
    bool support_color;

    // 初始化
    void (*init)(log_backend_t* backend);
    
    // 输出日志
    void (*output)(log_backend_t* backend, const log_record_t* record);
    
    // 指向下一个backend，形成链表，如果没有则为NULL
    log_backend_t* next;
};

// 全局默认日志级别，在此之前定义可以覆盖
#ifndef LOG_LEVEL_DEFAULT
#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO
#endif

// 日志缓冲区大小定义
#ifndef LOG_BUF_SIZE
#define LOG_BUF_SIZE 512
#endif

// 初始化日志系统
void log_init(void);
// 设置异步工作队列实例
// 默认不启用，如果启用则需要传入一个async_t对象指针
void log_set_async(void* async);
// 注册日志后端
void log_backend_register(log_backend_t* backend);
// 设置全局日志级别
void log_set_level(log_level_t level);
// 设置特定tag的日志级别
void log_set_tag_level(const char* tag, log_level_t level);
// 写入一个日志消息
// 在isr内也可以用，但是禁止在热路径使用，因为有vsprintf导致时间开销
void log_write(log_level_t level, const char* tag, const char* fmt, ...);

#define log_info(tag, fmt, ...) log_write(LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#define log_warn(tag, fmt, ...) log_write(LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#define log_error(tag, fmt, ...) log_write(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)

// 如果不使用异步，那么需要定期调用这个函数来输出日志
void log_output_all_backends_handler(void);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_LOG_LOG_H
