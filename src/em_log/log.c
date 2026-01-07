#include "log.h"
#include "ringbuffer.h"
#include "async.h"
#include "soft_timer.h"
#include "../em_arch/cpu_adapter.h"

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

#ifndef LOG_RB_SIZE
#    define LOG_RB_SIZE 2048
#endif

#ifndef LOG_FORMAT_BUF_SIZE
#    define LOG_FORMAT_BUF_SIZE 128
#endif

#ifndef LOG_MAX_TAG_FILTERS
#    define LOG_MAX_TAG_FILTERS 8
#endif

typedef struct
{
    const char* tag;
    log_level_t level;
} log_tag_filter_t;

static u8               g_log_rb_mem[LOG_RB_SIZE];
static ringbuffer_t     g_log_rb;
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
        if (b->enabled && rec->level <= b->min_level) {
            if (b->output)
                b->output(b, rec);
        }
        b = b->next;
    }
}

void log_init(void)
{
    ringbuffer_init(&g_log_rb, g_log_rb_mem, LOG_RB_SIZE);
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
    if (level > filter)
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
    if (ringbuffer_free(&g_log_rb) >= h.total_len) {
        ringbuffer_write(&g_log_rb, (u8*)&h, sizeof(h));
        ringbuffer_write(&g_log_rb, (u8*)buf, (u32)(n + 1));
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

void log_write_isr(log_level_t level, const char* tag, const char* fmt, int num_args, ...)
{
    if (level > g_log_level)
        return;
    if (num_args > 8)
        num_args = 8;

    uintptr_t args_cache[8];
    va_list   va;
    va_start(va, num_args);
    for (int i = 0; i < num_args; i++)
        args_cache[i] = va_arg(va, uintptr_t);
    va_end(va);

    log_packet_header_t h;
    u32                 now_ms = (u32)time_get_ms();
    h.time_sec                 = now_ms / 1000;
    h.time_ms                  = (u16)(now_ms % 1000);
    h.time_us                  = (u16)(time_get_us() % 1000);
    h.level                    = (u8)level;
    h.tag                      = tag;
    h.total_len = (u16)(sizeof(h) + sizeof(const char*) + (u16)(num_args * sizeof(uintptr_t)));

    CPU_ENTER_CRITICAL();
    if (ringbuffer_free(&g_log_rb) >= h.total_len) {
        ringbuffer_write(&g_log_rb, (u8*)&h, sizeof(h));
        ringbuffer_write(&g_log_rb, (u8*)&fmt, sizeof(const char*));
        for (int i = 0; i < num_args; i++)
            ringbuffer_write(&g_log_rb, (u8*)&args_cache[i], sizeof(uintptr_t));
    }
    else {
        g_log_drop_count++;
    }
    CPU_EXIT_CRITICAL();

    if (g_async && !g_log_task_active) {
        if (async_submit(g_async, log_process_task, NULL))
            g_log_task_active = true;
    }
}

static void log_process_task(void* arg)
{
    (void)arg;
    log_packet_header_t h;
    char                payload[LOG_BUF_SIZE];

    while (1) {
        CPU_ENTER_CRITICAL();
        if (ringbuffer_used(&g_log_rb) < sizeof(h)) {
            CPU_EXIT_CRITICAL();
            break;
        }
        ringbuffer_peek(&g_log_rb, (u8*)&h, sizeof(h));
        if (ringbuffer_used(&g_log_rb) < h.total_len) {
            CPU_EXIT_CRITICAL();
            break;
        }

        ringbuffer_read(&g_log_rb, (u8*)&h, sizeof(h));
        u16 pl_len = (u16)(h.total_len - sizeof(h));

        if (pl_len > sizeof(payload)) {
            ringbuffer_skip(&g_log_rb, pl_len);
            CPU_EXIT_CRITICAL();
            continue;
        }
        ringbuffer_read(&g_log_rb, (u8*)payload, pl_len);
        CPU_EXIT_CRITICAL();

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
    if (ringbuffer_used(&g_log_rb) > 0) {
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
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARN:
        return "WARN";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

#if TEST_ENABLE
#include "../em_test/test.h"
#include <string.h>

static int test_backend_calls;
static char test_payload[LOG_BUF_SIZE + 1];
static usize test_payload_len;
static const char* test_tag;
static log_level_t test_level;

static void test_backend_init(log_backend_t* b)
{
    (void)b;
    test_backend_calls = 0;
    test_payload_len = 0;
    test_tag = NULL;
}

static void test_backend_output(log_backend_t* b, const log_record_t* rec)
{
    (void)b;
    test_backend_calls++;
    test_level = rec->level;
    test_tag = rec->tag;
    test_payload_len = rec->payload_len;
    if (test_payload_len > LOG_BUF_SIZE)
        test_payload_len = LOG_BUF_SIZE;
    memcpy(test_payload, rec->payload, test_payload_len);
    test_payload[test_payload_len] = '\0';
}

static log_backend_t s_test_backend = {
    .name = "unit_test",
    .min_level = LOG_LEVEL_INFO,
    .enabled = true,
    .support_color = false,
    .init = test_backend_init,
    .output = test_backend_output,
    .next = NULL,
};

TEST_CASE(log_basic_output)
{
    log_init();
    log_set_level(LOG_LEVEL_INFO);
    log_backend_register(&s_test_backend);

    test_backend_calls = 0;
    log_write(LOG_LEVEL_INFO, "T1", "hello %d", 123);
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(1, test_backend_calls);
    TEST_ASSERT_EQUAL_INT(LOG_LEVEL_INFO, (int)test_level);
    TEST_ASSERT_EQUAL_STRING("T1", test_tag);
    TEST_ASSERT_EQUAL_INT(9, (int)test_payload_len); /* "hello 123" */
    TEST_ASSERT_EQUAL_STRING("hello 123", test_payload);
}

TEST_CASE(log_level_filter)
{
    log_init();
    log_backend_register(&s_test_backend);

    log_set_level(LOG_LEVEL_WARN);
    test_backend_calls = 0;

    log_write(LOG_LEVEL_INFO, "T1", "info");
    log_write(LOG_LEVEL_WARN, "T1", "warn");
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(1, test_backend_calls);
    TEST_ASSERT_EQUAL_STRING("warn", test_payload);
}

TEST_CASE(log_tag_filter)
{
    log_init();
    log_set_level(LOG_LEVEL_INFO);
    log_backend_register(&s_test_backend);

    log_set_tag_level("T2", LOG_LEVEL_WARN);
    test_backend_calls = 0;

    log_write(LOG_LEVEL_INFO, "T2", "info_T2");
    log_write(LOG_LEVEL_WARN, "T2", "warn_T2");
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(1, test_backend_calls);
    TEST_ASSERT_EQUAL_STRING("warn_T2", test_payload);
}

TEST_CASE(log_truncation)
{
    log_init();
    log_backend_register(&s_test_backend);

    /* create a long string to force truncation */
    char longstr[LOG_FORMAT_BUF_SIZE + 64];
    for (int i = 0; i < (int)sizeof(longstr) - 1; i++)
        longstr[i] = 'X';
    longstr[sizeof(longstr) - 1] = '\0';

    test_backend_calls = 0;
    log_write(LOG_LEVEL_INFO, "T3", "%s", longstr);
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(1, test_backend_calls);
    TEST_ASSERT_INT_WITHIN((int)(LOG_FORMAT_BUF_SIZE - 10), (int)(LOG_FORMAT_BUF_SIZE - 1), (int)test_payload_len);
    TEST_ASSERT_EQUAL_MEMORY(longstr, test_payload, (size_t)test_payload_len);
    TEST_ASSERT_EQUAL_INT((int)test_payload_len, (int)strlen(test_payload));
}

// demo

#define DHT11_TAG "DHT11"
#define dht11_log_info(fmt, ...) log_info(DHT11_TAG, fmt, ##__VA_ARGS__)
#define dht11_log_warn(fmt, ...) log_warn(DHT11_TAG, fmt, ##__VA_ARGS__)
#define dht11_log_error(fmt, ...) log_error(DHT11_TAG, fmt, ##__VA_ARGS__)

// 以控制台作为输出
void console_output(log_backend_t* backend, const log_record_t* record)
{
    // 按照时间的方式打印
    // [seconds.ms.us] [TAG] [LEVEL]: payload
    printf("[%lu.%03u.%03u] [%s] [%s]: %.*s\n",
        record->time_sec,
        record->time_ms,
        record->time_us,
        record->tag ? record->tag : "NO_TAG",
        log_level_to_string(record->level),
        (int)record->payload_len,
        record->payload);
}

log_backend_t console_backend = {
    .name = "dht11_console",
    .min_level = LOG_LEVEL_INFO,
    .enabled = true,
    .support_color = false,
    .init = NULL,
    .output = console_output,
    .next = NULL,
};

TEST_CASE(log_demo_dht11_module_log)
{
    // 使用默认的日志级别
    log_init();
   
    log_backend_register(&s_test_backend);

    dht11_log_info("DHT11 initialized");
    dht11_log_warn("DHT11 read timeout");
    dht11_log_error("DHT11 checksum error");

    log_output_all_backends_handler();
}

#endif /* TEST_ENABLE */
