#include "trace.h"
#include "log.h"
#include "../em_dstream/ring_buffer.h"
#include "async.h"
#include "soft_timer.h"
#include "../em_arch/cpu_adapter.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION g_trace_cs;
static bool g_trace_cs_init = false;
#define TRACE_ENTER_CRITICAL() do { \
    if (!g_trace_cs_init) { InitializeCriticalSection(&g_trace_cs); g_trace_cs_init = true; } \
    EnterCriticalSection(&g_trace_cs); \
} while(0)
#define TRACE_EXIT_CRITICAL() LeaveCriticalSection(&g_trace_cs)
#else
#define TRACE_ENTER_CRITICAL() CPU_ENTER_CRITICAL()
#define TRACE_EXIT_CRITICAL() CPU_EXIT_CRITICAL()
#endif

#ifndef TRACE_RB_SIZE
#define TRACE_RB_SIZE 4096
#endif

#ifndef TRACE_MAX_STRINGS
#define TRACE_MAX_STRINGS 128
#endif

#pragma pack(push, 1)
typedef struct {
    uint32_t time_sec;
    uint16_t time_ms;
    uint16_t time_us;
    trace_id_t name_id;
    uint32_t line;
    uint64_t value;
    const void* func_addr;
} trace_packet_t;
#pragma pack(pop)

static uint8_t g_trace_rb_mem[TRACE_RB_SIZE];
static ring_buffer_t g_trace_rb;
static const char* g_trace_strings[TRACE_MAX_STRINGS];
static volatile int g_trace_string_count = 0;
static async_t* g_trace_async = NULL;
static volatile bool g_trace_task_active = false;

static void trace_process_task(void* arg);

void trace_init(void) {
    ring_buf_init(&g_trace_rb, g_trace_rb_mem, TRACE_RB_SIZE);
    g_trace_string_count = 0;
    g_trace_task_active = false;
}

void trace_set_async(void* async) {
    g_trace_async = (async_t*)async;
}

trace_id_t trace_register(const char* name) {
    if (!name) return 0;
    
    TRACE_ENTER_CRITICAL();
    // Simple linear search to avoid duplicates (optional, but good)
    for (int i = 0; i < g_trace_string_count; i++) {
        if (g_trace_strings[i] == name || strcmp(g_trace_strings[i], name) == 0) {
            TRACE_EXIT_CRITICAL();
            return (trace_id_t)i;
        }
    }
    
    if (g_trace_string_count < TRACE_MAX_STRINGS) {
        g_trace_strings[g_trace_string_count] = name;
        trace_id_t id = (trace_id_t)g_trace_string_count;
        g_trace_string_count++;
        TRACE_EXIT_CRITICAL();
        return id;
    }
    
    TRACE_EXIT_CRITICAL();
    return 0xFFFF; // Error ID
}

void trace_write(trace_id_t id, uint64_t value, const void* func_addr, uint32_t line) {
    trace_packet_t packet;
    uint32_t now_ms = time_get_ms();
    uint16_t now_us_offset = (uint16_t)(time_get_us() % 1000);

    packet.time_sec = now_ms / 1000;
    packet.time_ms  = (uint16_t)(now_ms % 1000);
    packet.time_us  = now_us_offset;
    packet.name_id = id;
    packet.value = value;
    packet.func_addr = func_addr;
    packet.line = line;

    TRACE_ENTER_CRITICAL();
    if (ring_buf_free(&g_trace_rb) >= sizeof(packet)) {
        ring_buf_write(&g_trace_rb, (uint8_t*)&packet, sizeof(packet));
        
        if (g_trace_async && !g_trace_task_active) {
            if (async_submit(g_trace_async, trace_process_task, NULL)) {
                g_trace_task_active = true;
            }
        }
    }
    TRACE_EXIT_CRITICAL();
}

static void trace_process_task(void* arg) {
    (void)arg;
    trace_packet_t packet;
    char payload_buf[256];

    while (1) {
        TRACE_ENTER_CRITICAL();
        if (ring_buf_used(&g_trace_rb) < sizeof(packet)) {
            TRACE_EXIT_CRITICAL();
            break;
        }
        ring_buf_read(&g_trace_rb, (uint8_t*)&packet, sizeof(packet));
        TRACE_EXIT_CRITICAL();

        // Format
        const char* name = "UNKNOWN";
        if (packet.name_id < g_trace_string_count) {
            name = g_trace_strings[packet.name_id];
        }

        // Format: [TRACE] Name: Value (Func:Line)
        int len = snprintf(payload_buf, sizeof(payload_buf), 
            "[TRACE] %s: %llu (Func:%p Line:%u)", 
            name, packet.value, packet.func_addr, packet.line);

        log_record_t record;
        record.level = LOG_LEVEL_INFO; 
        record.time_sec = packet.time_sec;
        record.time_ms  = packet.time_ms;
        record.time_us  = packet.time_us;
        record.tag = "TRACE";
        record.payload = payload_buf;
        record.payload_len = len;

        // log_output_all_backends_handler(&record);
    }

    TRACE_ENTER_CRITICAL();
    g_trace_task_active = false;
    if (ring_buf_used(&g_trace_rb) > 0) {
        if (g_trace_async && async_submit(g_trace_async, trace_process_task, NULL)) {
            g_trace_task_active = true;
        }
    }
    TRACE_EXIT_CRITICAL();
}

void trace_process(void) {
    trace_process_task(NULL);
}

void trace_flush(void) {
    trace_process();
}
