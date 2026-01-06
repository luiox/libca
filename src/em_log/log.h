#pragma once

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
    uint32_t time_sec;    /**< Seconds */
    uint16_t time_ms;     /**< Milliseconds */
    uint16_t time_us;     /**< Microseconds */
    const char* tag;      /**< Tag string (must be static/persistent) */
    const char* payload;  /**< Formatted log message */
    size_t payload_len;   /**< Length of payload */
} log_record_t;

typedef struct log_backend log_backend_t;

/**
 * @brief Log backend interface
 */
struct log_backend {
    const char* name;
    log_level_t min_level; /**< Minimum level to output for this backend */
    bool enabled;             /**< Enable/Disable this backend */
    bool support_color;       /**< Whether this backend supports ANSI colors */
    
    /** Initialize the backend */
    void (*init)(log_backend_t* backend);
    
    /** Output a log record (Normal context) */
    void (*output)(log_backend_t* backend, const log_record_t* record);
    
    /** Output a log record (Panic/ISR context) - MUST be polling/blocking */
    void (*panic_output)(log_backend_t* backend, const log_record_t* record);
    
    /** Flush any buffered data */
    void (*flush)(log_backend_t* backend);
    
    log_backend_t* next;   /**< Linked list pointer (managed by log) */
};

/* Configuration Macros (Can be overridden by build system) */
#ifndef LOG_LEVEL_DEFAULT
#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO
#endif

#ifndef LOG_BUF_SIZE
#define LOG_BUF_SIZE 128
#endif

/* Public API */

/**
 * @brief Initialize the logging system
 */
void log_init(void);

/**
 * @brief Set the async context for background processing
 * @param async Pointer to the async context
 */
void log_set_async(void* async);

/**
 * @brief Register a new backend
 * @param backend Pointer to the backend structure (must be persistent)
 */
void log_backend_register(log_backend_t* backend);

/**
 * @brief Set the global log level filter
 * @param level Minimum level to process
 */
void log_set_level(log_level_t level);

/**
 * @brief Set the log level for a specific tag
 * @param tag Tag string (must be persistent/static)
 * @param level Minimum level to process for this tag
 */
void log_set_tag_level(const char* tag, log_level_t level);

/**
 * @brief Write a log message
 * @param level Log level
 * @param tag Log tag (must be a static string)
 * @param fmt Format string
 * @param ... Arguments
 */
void log_write(log_level_t level, const char* tag, const char* fmt, ...);

/**
 * @brief Write a log message from ISR (Deferred formatting)
 * @param level Log level
 * @param tag Log tag (must be a static string)
 * @param fmt Format string (must be a static string)
 * @param num_args Number of integer arguments (0-4)
 * @param ... Integer arguments (uintptr_t)
 */
void log_write_isr(log_level_t level, const char* tag, const char* fmt, int num_args, ...);

/* Helper macros for ISR logging to auto-count arguments */
// MSVC requires a workaround for __VA_ARGS__ expansion
#define _LOG_EXPAND(x) x
#define _LOG_NARG(...) _LOG_EXPAND(_LOG_NARG_(__VA_ARGS__, _LOG_RSEQ_N()))
#define _LOG_NARG_(...) _LOG_EXPAND(_LOG_ARG_N(__VA_ARGS__))
#define _LOG_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N
#define _LOG_RSEQ_N() 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#define LOG_ISR_I(tag, fmt, ...) log_write_isr(LOG_LEVEL_INFO, tag, fmt, _LOG_NARG(__VA_ARGS__), ##__VA_ARGS__)
#define LOG_ISR_W(tag, fmt, ...) log_write_isr(LOG_LEVEL_WARN, tag, fmt, _LOG_NARG(__VA_ARGS__), ##__VA_ARGS__)
#define LOG_ISR_E(tag, fmt, ...) log_write_isr(LOG_LEVEL_ERROR, tag, fmt, _LOG_NARG(__VA_ARGS__), ##__VA_ARGS__)

/**
 * @brief Flush all logs (synchronous)
 */
void log_flush(void);

/**
 * @brief Panic mode: flush logs and switch to synchronous output
 */
void log_panic(void);

/**
 * @brief Output a record to all registered backends
 * @param record The log record to output
 */
void log_output_all_backends(const log_record_t* record);

/* Convenience Macros */
#if LOG_LEVEL_DEFAULT >= LOG_LEVEL_ERROR
#define LOG_E(tag, ...) log_write(LOG_LEVEL_ERROR, tag, __VA_ARGS__)
#else
#define LOG_E(tag, ...) ((void)0)
#endif

#if LOG_LEVEL_DEFAULT >= LOG_LEVEL_WARN
#define LOG_W(tag, ...) log_write(LOG_LEVEL_WARN, tag, __VA_ARGS__)
#else
#define LOG_W(tag, ...) ((void)0)
#endif

#if LOG_LEVEL_DEFAULT >= LOG_LEVEL_INFO
#define LOG_I(tag, ...) log_write(LOG_LEVEL_INFO, tag, __VA_ARGS__)
#else
#define LOG_I(tag, ...) ((void)0)
#endif

/* Helper macros for stringification */
#define _LOG_STR(x) #x
#define _LOG_XSTR(x) _LOG_STR(x)

#ifdef __cplusplus
}
#endif
