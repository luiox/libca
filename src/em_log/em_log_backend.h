#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log levels
 */
typedef enum {
    EM_LOG_NONE = 0,
    EM_LOG_ERROR,
    EM_LOG_WARN,
    EM_LOG_INFO,
    EM_LOG_DEBUG,
    EM_LOG_VERBOSE
} em_log_level_t;

/**
 * @brief Log record structure passed to backends
 */
typedef struct {
    em_log_level_t level;
    uint32_t time_sec;    /**< Seconds */
    uint16_t time_ms;     /**< Milliseconds */
    uint16_t time_us;     /**< Microseconds */
    const char* tag;      /**< Tag string (must be static/persistent) */
    const char* payload;  /**< Formatted log message */
    size_t payload_len;   /**< Length of payload */
} em_log_record_t;

typedef struct em_log_backend em_log_backend_t;

/**
 * @brief Log backend interface
 */
struct em_log_backend {
    const char* name;
    em_log_level_t min_level; /**< Minimum level to output for this backend */
    bool enabled;             /**< Enable/Disable this backend */
    bool support_color;       /**< Whether this backend supports ANSI colors */
    
    /** Initialize the backend */
    void (*init)(em_log_backend_t* backend);
    
    /** Output a log record (Normal context) */
    void (*output)(em_log_backend_t* backend, const em_log_record_t* record);
    
    /** Output a log record (Panic/ISR context) - MUST be polling/blocking */
    void (*panic_output)(em_log_backend_t* backend, const em_log_record_t* record);
    
    /** Flush any buffered data */
    void (*flush)(em_log_backend_t* backend);
    
    em_log_backend_t* next;   /**< Linked list pointer (managed by em_log) */
};

#ifdef __cplusplus
}
#endif
