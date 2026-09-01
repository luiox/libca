#ifndef LIBCA_EM_LOG_TRACE_H
#define LIBCA_EM_LOG_TRACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t trace_id_t;

/// @brief Initialize the trace system
void trace_init(void);

/// @brief Set the async context for background processing
/// @param async Pointer to the async context
void trace_set_async(void* async);

/// @brief Register a trace data name
/// @param name Static string pointer representing the data name
/// @return Trace ID
trace_id_t trace_register(const char* name);

/// @brief Write a trace record
/// @param id Trace ID (from trace_register)
/// @param value Data value
/// @param func_addr Function address (or name string pointer)
/// @param line Line number
void trace_write(trace_id_t id, uint64_t value, const void* func_addr, uint32_t line);

/// @brief Process trace buffer (called by async worker)
void trace_process(void);

/// @brief Flush trace buffer
void trace_flush(void);

/* Macros */

// Helper to get current function address or name
#if defined(__GNUC__) || defined(__clang__)
#    define TRACE_GET_FUNC() __builtin_return_address(0)
#else
#    define TRACE_GET_FUNC() __FUNCTION__
#endif

#define TRACE(id, val) trace_write(id, (uint64_t)(val), TRACE_GET_FUNC(), __LINE__)

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_LOG_TRACE_H
