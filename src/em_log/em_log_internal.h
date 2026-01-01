#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct {
    uint16_t total_len;  /**< Total length of the packet (Header + Payload) */
    uint8_t  level;      /**< Log level */
    uint8_t  type;       /**< Packet type: 0=String, 1=ISR Args */
    uint8_t  num_args;   /**< Number of arguments (Only for ISR type) */
    uint32_t time_sec;   /**< Seconds since boot */
    uint16_t time_ms;    /**< Milliseconds part (0-999) */
    uint16_t time_us;    /**< Microseconds part (0-999) */
    const char* tag;     /**< Pointer to static tag string */
} log_packet_header_t;

#define LOG_PKT_TYPE_STRING   0
#define LOG_PKT_TYPE_ISR_ARGS 1
#pragma pack(pop)

#ifdef __cplusplus
}
#endif
