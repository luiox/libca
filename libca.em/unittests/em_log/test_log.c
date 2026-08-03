/* Auto-migrated from src/em_log/log.c test blocks */
#include "log.h"
#include <em_dstream/ring_buffer.h>
#include <em_platform/async.h>
#include <em_platform/time_util.h>
#include <em_platform/cpu_adapter.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <em_test/test.h>
#include <string.h>
#include <time.h>

#define LOG_BUF_SIZE 256

static int test_backend_calls;
static char test_payload[LOG_BUF_SIZE + 1];
static usize test_payload_len;
static const char* test_tag;
static log_level_t test_level;
static u32 test_time_sec;
static u16 test_time_ms;
static u16 test_time_us;

static void test_backend_init(log_backend_t* b)
{
    (void)b;
    test_backend_calls = 0;
    test_payload_len = 0;
    test_tag = NULL;
    test_time_sec = 0;
    test_time_ms = 0;
    test_time_us = 0;
}

static void test_backend_output(log_backend_t* b, const log_record_t* rec)
{
    (void)b;
    test_backend_calls++;
    test_level = rec->level;
    test_tag = rec->tag;
    test_time_sec = rec->time_sec;
    test_time_ms = rec->time_ms;
    test_time_us = rec->time_us;
    test_payload_len = rec->payload_len;
    if (test_payload_len > LOG_BUF_SIZE)
        test_payload_len = LOG_BUF_SIZE;
    memcpy(test_payload, rec->payload, test_payload_len);
    test_payload[test_payload_len] = '\0';
}

// 使用一个线程加随机数模拟时间更新
// 用 g_time_thread_stop 标志让线程优雅退出，避免 pthread_cancel/TerminateThread
// 在持锁/IO 中途杀线程引发的未定义行为（Linux CI 曾因此 SIGSEGV）。
static volatile int g_time_thread_stop = 0;

#ifdef _WIN32
#include <windows.h>
#include <process.h>
static void __cdecl log_time_update_thread(void* arg) {
    (void)arg;
    while (!g_time_thread_stop) {
        time_update_tick_ms(10);
        Sleep(10);
    }
}
#else
#ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#endif
#include <time.h>
#include <pthread.h>
#include <unistd.h>
static void* log_time_update_thread(void* arg) {
    (void)arg;
    struct timespec ts = {0, 10 * 1000000}; /* 10ms */
    while (!g_time_thread_stop) {
        time_update_tick_ms(10);
        nanosleep(&ts, NULL);
    }
    return NULL;
}
#endif

#include <stdlib.h>
#include <time.h>

// 模拟一个get_us函数，返回带随机抖动的微秒时间
// 为了防止在同一个ms内多次调用，可能前一次比后一次大，每次调用后推进1ms
static u32 test_us_provider(void) {
    u32 ms = time_get_ms();
    u32 val = (u32)(ms * 1000 + (u32)(rand() % 1000));
    /* advance ms to make subsequent readings vary */
    time_update_tick_ms(1);
    return val;
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

TEST_CASE(log_time_provider)
{
    log_init();
    log_set_level(LOG_LEVEL_INFO);
    log_backend_register(&s_test_backend);

    /* 使用手动更新时间线程替代静态时间提供者 */
    time_set_ms_provider(NULL);
    time_update_tick_ms(0);

    /* 初始化随机数种子并设置带随机抖动的微秒时间提供者 */
    srand((unsigned)time(NULL));
    time_set_us_provider((time_get_fn_t)test_us_provider);

    g_time_thread_stop = 0;
#ifdef _WIN32
    HANDLE th = (HANDLE)_beginthread(log_time_update_thread, 0, NULL);
    /* 等待线程运行一会 */
    Sleep(50);
#else
    pthread_t th;
    pthread_create(&th, NULL, log_time_update_thread, NULL);
    /* 等待线程运行一会 */
    usleep(50000);
#endif

    test_backend_calls = 0;
    log_write(LOG_LEVEL_INFO, "Ttime", "tick");
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(1, test_backend_calls);
    TEST_ASSERT_TRUE(test_payload_len > 0);
    TEST_ASSERT_INT_WITHIN(0, 999, (int)test_time_ms);
    TEST_ASSERT_INT_WITHIN(0, 999, (int)test_time_us);
    TEST_ASSERT_TRUE(test_time_sec >= 0);

    /* 优雅停止时间线程：置标志 + join，避免 cancel/terminate 杀持锁线程 */
    g_time_thread_stop = 1;
#ifdef _WIN32
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
#else
    pthread_join(th, NULL);
#endif
}

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
    TEST_ASSERT_EQUAL_INT(9, (int)test_payload_len); /* "hello 123" 字符串长度为9 */
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

// 测试：当 filter 使用与消息 tag 指针不同但字符串相等的 tag（例如 strdup），应通过 strcmp 正确匹配过滤
TEST_CASE(log_tag_filter_strcmp_path)
{
    log_init();
    log_set_level(LOG_LEVEL_INFO);
    log_backend_register(&s_test_backend);

    /* 使用 strdup 创建一个与字面量相同内容但不同指针的 tag */
    char* dyn = strdup("T2");
    log_set_tag_level(dyn, LOG_LEVEL_WARN);

    test_backend_calls = 0;

    /* 这里使用字面量 "T2" 写日志，指针与上面的 dyn 不相同，但字符串内容一致 */
    log_write(LOG_LEVEL_INFO, "T2", "info_T2");
    log_write(LOG_LEVEL_WARN, "T2", "warn_T2");
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(1, test_backend_calls);
    TEST_ASSERT_EQUAL_STRING("warn_T2", test_payload);

    free(dyn);
}

TEST_CASE(log_truncation)
{
    log_init();
    log_backend_register(&s_test_backend);

    /* 构造一个超长字符串以触发截断 */
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

// 测试：注册一个 min_level = LOG_LEVEL_WARN 的后端，写入 INFO 与 WARN，断言只有 WARN 被输出
TEST_CASE(log_backend_min_level)
{
    log_init();
    log_set_level(LOG_LEVEL_INFO);
    log_backend_t b = s_test_backend;
    b.min_level = LOG_LEVEL_WARN;
    b.enabled = true;
    b.init = NULL;
    b.output = test_backend_output;
    b.next = NULL;

    /* 注册本地后端实例 */
    log_backend_register(&b);

    test_backend_calls = 0;
    log_write(LOG_LEVEL_INFO, "TB", "info");
    log_write(LOG_LEVEL_WARN, "TB", "warn");
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(1, test_backend_calls);
    TEST_ASSERT_EQUAL_STRING("warn", test_payload);
}

// 测试：设置后端 enabled = false，写入 INFO，断言后端不被调用
TEST_CASE(log_backend_enabled)
{
    log_init();
    log_set_level(LOG_LEVEL_INFO);
    log_backend_t b = s_test_backend;
    b.min_level = LOG_LEVEL_INFO;
    b.enabled = false;
    b.init = NULL;
    b.output = test_backend_output;
    b.next = NULL;

    /* 注册本地后端实例 */
    log_backend_register(&b);

    test_backend_calls = 0;
    log_write(LOG_LEVEL_INFO, "TB", "info");
    log_output_all_backends_handler();

    TEST_ASSERT_EQUAL_INT(0, test_backend_calls);
}

// 示例

#define DHT11_TAG "DHT11"
#define dht11_log_info(fmt, ...) log_info(DHT11_TAG, fmt, ##__VA_ARGS__)
#define dht11_log_warn(fmt, ...) log_warn(DHT11_TAG, fmt, ##__VA_ARGS__)
#define dht11_log_error(fmt, ...) log_error(DHT11_TAG, fmt, ##__VA_ARGS__)

// 以控制台作为输出
void console_output(log_backend_t* backend, const log_record_t* record)
{
    // 按照时间的方式打印
    // [seconds.ms.us] [TAG] [LEVEL]: payload
    printf("%s[%03u.%03u.%03u] [%s] [%s]: %.*s%s\n",
        backend->support_color ? log_level_to_ansi_color(record->level) : "",
        record->time_sec,
        record->time_ms,
        record->time_us,
        record->tag ? record->tag : "NO_TAG",
        log_level_to_string(record->level),
        (int)record->payload_len,
        record->payload,
        // 去除颜色
        backend->support_color ? "\x1b[0m" : ""    
    );
}

log_backend_t dht11_logger = {
    .name = "dht11_console",
    .min_level = LOG_LEVEL_INFO,
    .enabled = true,
    .support_color = true,
    .init = NULL,
    .output = console_output,
    .next = NULL,
};

TEST_CASE(log_demo_dht11_module_log)
{
    // 使用默认的日志级别
    log_init();
   
    log_backend_register(&dht11_logger);
    dht11_log_info("DHT11 initialized");
    dht11_log_warn("DHT11 read timeout");
    dht11_log_error("DHT11 checksum error");

    log_output_all_backends_handler();
}

