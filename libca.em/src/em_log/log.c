#include "log.h"
#include <em_dstream/ring_buffer.h>
#include <em_platform/async.h>
#include <em_platform/time_util.h>
#include <em_platform/cpu_adapter.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#pragma pack(push, 1)
typedef struct
{
    u16         total_len; /* total bytes (header + payload) */
    u8          level;     /* log_level_t */
    u32         time_sec;
    u16         time_ms;
    u16         time_us;
    const char* tag;
} log_packet_header_t;
#pragma pack(pop)

typedef struct
{
    const char* tag;
    log_level_t level;
} log_tag_filter_t;

static u8               g_log_rb_mem[LOG_RB_SIZE];
static ring_buffer_t    g_log_rb;
static log_backend_t*   g_backend_list = NULL;
static log_level_t      g_log_level    = LOG_LEVEL_DEFAULT;
static log_tag_filter_t g_tag_filters[LOG_MAX_TAG_FILTERS];
static int              g_tag_filter_count = 0;
static async_t*         g_async            = NULL;
static volatile bool    g_log_task_active  = false;
static volatile u32     g_log_drop_count   = 0;

static void log_process_task(void* arg);

static void log_dispatch_to_backends(const log_record_t* rec)
{
    log_backend_t* b = g_backend_list;
    while (b) {

        if (b->enabled && rec->level >= b->min_level) {
            if (b->output)
                b->output(b, rec);
        }
        b = b->next;
    }
}

void log_init(void)
{
    ring_buf_init(&g_log_rb, g_log_rb_mem, LOG_RB_SIZE);
    g_backend_list     = NULL;
    g_log_task_active  = false;
    g_log_drop_count   = 0;
    g_tag_filter_count = 0;
}

void log_set_async(void* async)
{
    g_async = (async_t*)async;
}

void log_backend_register(log_backend_t* backend)
{
    if (!backend)
        return;
    if (backend->init)
        backend->init(backend);

    CPU_ENTER_CRITICAL();
    backend->next  = g_backend_list;
    g_backend_list = backend;
    CPU_EXIT_CRITICAL();
}

void log_set_level(log_level_t level)
{
    g_log_level = level;
}

// 注意：tag 必须为静态字符串（例如字面量或全局常量）。
// 实现将直接保存 `const char*` 指针到内部表和日志包中，
// 如果传入动态或栈上字符串，释放后会导致未定义行为（悬空指针）。
// 为了简化设计，后端建议在系统初始化阶段注册并保持不变；
// 本实现假设后端只在初始化时注册，不支持运行时并发注册/注销。
void log_set_tag_level(const char* tag, log_level_t level)
{
    if (!tag)
        return;
    CPU_ENTER_CRITICAL();
    for (int i = 0; i < g_tag_filter_count; i++) {
        if (g_tag_filters[i].tag == tag || strcmp(g_tag_filters[i].tag, tag) == 0) {
            g_tag_filters[i].level = level;
            CPU_EXIT_CRITICAL();
            return;
        }
    }
    if (g_tag_filter_count < LOG_MAX_TAG_FILTERS) {
        g_tag_filters[g_tag_filter_count].tag   = tag;
        g_tag_filters[g_tag_filter_count].level = level;
        g_tag_filter_count++;
    }
    CPU_EXIT_CRITICAL();
}

void log_write(log_level_t level, const char* tag, const char* fmt, ...)
{
    log_level_t filter = g_log_level;
    if (g_tag_filter_count > 0 && tag) {
        for (int i = 0; i < g_tag_filter_count; i++) {
            if (g_tag_filters[i].tag == tag || strcmp(g_tag_filters[i].tag, tag) == 0) {
                filter = g_tag_filters[i].level;
                break;
            }
        }
    }
    if (level < filter)
        return;

    log_packet_header_t h;
    u32                 now_ms = (u32)time_get_ms();
    h.time_sec                 = now_ms / 1000;
    h.time_ms                  = (u16)(now_ms % 1000);
    h.time_us                  = (u16)(time_get_us() % 1000);
    h.level                    = (u8)level;
    h.tag                      = tag;

    char    buf[LOG_FORMAT_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        n = 0;
    if (n >= (int)sizeof(buf))
        n = (int)sizeof(buf) - 1;

    h.total_len = (u16)(sizeof(h) + (u16)(n + 1));

    CPU_ENTER_CRITICAL();
    if (ring_buf_free(&g_log_rb) >= h.total_len) {
        ring_buf_write(&g_log_rb, (u8*)&h, sizeof(h));
        ring_buf_write(&g_log_rb, (u8*)buf, (u32)(n + 1));
        if (g_async && !g_log_task_active) {
            if (async_submit(g_async, log_process_task, NULL))
                g_log_task_active = true;
        }
    }
    else {
        g_log_drop_count++;
    }
    CPU_EXIT_CRITICAL();
}

static void log_process_task(void* arg)
{
    (void)arg;
    log_packet_header_t h;
    char                payload[256];

    while (1) {
        CPU_ENTER_CRITICAL();
        if (ring_buf_used(&g_log_rb) < sizeof(h)) {
            CPU_EXIT_CRITICAL();
            break;
        }
        ring_buf_peek(&g_log_rb, (u8*)&h, sizeof(h));
        if (ring_buf_used(&g_log_rb) < h.total_len) {
            CPU_EXIT_CRITICAL();
            break;
        }

#if ENABLE_DEBUG_OUTPUT
        printf("[log_debug] used=%u total_len=%u\n",
               (unsigned)ring_buf_used(&g_log_rb),
               (unsigned)h.total_len);
#endif

        ring_buf_read(&g_log_rb, (u8*)&h, sizeof(h));
        u16 pl_len = (u16)(h.total_len - sizeof(h));

#if ENABLE_DEBUG_OUTPUT
        printf("[log_debug] after read header total_len=%u pl_len=%u\n",
               (unsigned)h.total_len,
               (unsigned)pl_len);
#endif

        if (pl_len > sizeof(payload)) {
            ring_buf_skip(&g_log_rb, pl_len);
            CPU_EXIT_CRITICAL();
            continue;
        }
        ring_buf_read(&g_log_rb, (u8*)payload, pl_len);
        CPU_EXIT_CRITICAL();

#if ENABLE_DEBUG_OUTPUT
        printf("[log_debug] payload_len=%u tag=%p\n",
               (unsigned)(pl_len > 0 ? (pl_len - 1) : 0),
               (void*)h.tag);
#endif

        log_record_t rec;
        rec.level       = (log_level_t)h.level;
        rec.time_sec    = h.time_sec;
        rec.time_ms     = h.time_ms;
        rec.time_us     = h.time_us;
        rec.tag         = h.tag;
        rec.payload     = payload;
        rec.payload_len = pl_len > 0 ? (usize)(pl_len - 1) : 0;

        log_dispatch_to_backends(&rec);
    }

    CPU_ENTER_CRITICAL();
    g_log_task_active = false;
    if (ring_buf_used(&g_log_rb) > 0) {
        if (g_async && async_submit(g_async, log_process_task, NULL))
            g_log_task_active = true;
    }
    CPU_EXIT_CRITICAL();
}

void log_output_all_backends_handler(void)
{
    log_process_task(NULL);
}

const char* log_level_to_string(log_level_t level)
{
    switch (level) {
    case LOG_LEVEL_INFO: return "INFO";
    case LOG_LEVEL_WARN: return "WARN";
    case LOG_LEVEL_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

const char* log_level_to_ansi_color(log_level_t level)
{
    switch (level) {
    case LOG_LEVEL_INFO: return "\x1b[32m";    // 绿色
    case LOG_LEVEL_WARN: return "\x1b[33m";    // 黄色
    case LOG_LEVEL_ERROR: return "\x1b[31m";   // 红色
    default: return "\x1b[0m";                 // 重置
    }
}
