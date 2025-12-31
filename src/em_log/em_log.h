#pragma once

#include "em_log_backend.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration Macros (Can be overridden by build system) */
#ifndef EM_LOG_LEVEL_DEFAULT
#define EM_LOG_LEVEL_DEFAULT EM_LOG_VERBOSE
#endif

#ifndef EM_LOG_BUF_SIZE
#define EM_LOG_BUF_SIZE 128
#endif

/* Public API */

/**
 * @brief Initialize the logging system
 */
void em_log_init(void);

/**
 * @brief Set the async context for background processing
 * @param async Pointer to the async context
 */
void em_log_set_async(void* async);

/**
 * @brief Register a new backend
 * @param backend Pointer to the backend structure (must be persistent)
 */
void em_log_backend_register(em_log_backend_t* backend);

/**
 * @brief Set the global log level filter
 * @param level Minimum level to process
 */
void em_log_set_level(em_log_level_t level);

/**
 * @brief Set the log level for a specific tag
 * @param tag Tag string (must be persistent/static)
 * @param level Minimum level to process for this tag
 */
void em_log_set_tag_level(const char* tag, em_log_level_t level);

/**
 * @brief Write a log message
 * @param level Log level
 * @param tag Log tag (must be a static string)
 * @param fmt Format string
 * @param ... Arguments
 */
void em_log_write(em_log_level_t level, const char* tag, const char* fmt, ...);

/**
 * @brief Flush all logs (synchronous)
 */
void em_log_flush(void);

/**
 * @brief Panic mode: flush logs and switch to synchronous output
 */
void em_log_panic(void);

/* Convenience Macros */
#if EM_LOG_LEVEL_DEFAULT >= EM_LOG_ERROR
#define EM_LOG_E(tag, ...) em_log_write(EM_LOG_ERROR, tag, __VA_ARGS__)
#else
#define EM_LOG_E(tag, ...) ((void)0)
#endif

#if EM_LOG_LEVEL_DEFAULT >= EM_LOG_WARN
#define EM_LOG_W(tag, ...) em_log_write(EM_LOG_WARN, tag, __VA_ARGS__)
#else
#define EM_LOG_W(tag, ...) ((void)0)
#endif

#if EM_LOG_LEVEL_DEFAULT >= EM_LOG_INFO
#define EM_LOG_I(tag, ...) em_log_write(EM_LOG_INFO, tag, __VA_ARGS__)
#else
#define EM_LOG_I(tag, ...) ((void)0)
#endif

/* Helper macros for stringification */
#define _EM_LOG_STR(x) #x
#define _EM_LOG_XSTR(x) _EM_LOG_STR(x)

#if EM_LOG_LEVEL_DEFAULT >= EM_LOG_DEBUG
#define EM_LOG_D(tag, fmt, ...) em_log_write(EM_LOG_DEBUG, tag, "[" __FILE__ ":" _EM_LOG_XSTR(__LINE__) "] " fmt, ##__VA_ARGS__)
#else
#define EM_LOG_D(tag, fmt, ...) ((void)0)
#endif

#if EM_LOG_LEVEL_DEFAULT >= EM_LOG_VERBOSE
#define EM_LOG_V(tag, fmt, ...) em_log_write(EM_LOG_VERBOSE, tag, "[" __FILE__ ":" _EM_LOG_XSTR(__LINE__) "] " fmt, ##__VA_ARGS__)
#else
#define EM_LOG_V(tag, fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif
