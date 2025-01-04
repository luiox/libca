#ifndef LOG_H
#define LOG_H

#include <libca/stdtype.h>

typedef enum { 
    LOG_DEBUG, 
    LOG_INFO,
    LOG_WARN, 
    LOG_ERROR,
}log_level_enum;

typedef struct{
    void* buf; // 需要一块内存来作为buf
    uint32_t buf_size; // buf的大小
}logger_t;

void log_init(void* buf, uint32_t buf_size);

void log_log(int32_t level, const char *file, int32_t line, const char *fmt, ...);

#endif // !LOG_H
