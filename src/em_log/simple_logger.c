#include "simple_logger.h"
#include "em_format/format.h"
#include <stdarg.h>
#include <stdio.h> // 仅用于 vsnprintf

#if TEST_ENABLE
#include <string.h>
#endif

static slog_output_fn_t g_out_fn = NULL;

#if SLOG_ENABLE_RUNTIME_LEVEL
static u8 g_runtime_level = LOG_LEVEL_DEBUG;
#endif

/**
 * @brief 统一格式化入口，可在标准 vsnprintf 与 fast_vsnprintf 之间切换
 */
static i32 slog_vsnprintf(char *buf, usize buf_size, const char *fmt, va_list args)
{
#if SLOG_USE_FAST_VSNPRINTF
    return fmt_vsnprintf(buf, buf_size, fmt, args);
#else
    int len = vsnprintf(buf, buf_size, fmt, args);
    if (len < 0) {
        return -1;
    }
    return (i32)len;
#endif
}

void slog_init(slog_output_fn_t out_fn) {
    SLOG_LOCK_ENTER();
    g_out_fn = out_fn;
    SLOG_LOCK_EXIT();
}

#if SLOG_ENABLE_RUNTIME_LEVEL
void slog_set_runtime_level(u8 level)
{
    if (level > LOG_LEVEL_DEBUG) {
        level = LOG_LEVEL_DEBUG;
    }
    g_runtime_level = level;
}

u8 slog_get_runtime_level(void)
{
    return g_runtime_level;
}

bool slog_should_log(u8 level)
{
    return g_runtime_level >= level;
}
#endif

// 唯一的底层函数
void _slog_printf(const char *fmt, ...) {
    if (fmt == NULL) {
        return;
    }

    SLOG_LOCK_ENTER();
    if (g_out_fn == NULL) {
        SLOG_LOCK_EXIT();
        return;
    }

    char buf[SLOG_BUFFER_SIZE];
    va_list args;
    
    va_start(args, fmt);
    i32 len = slog_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0) {
        SLOG_LOCK_EXIT();
        return;
    }

    // 防止缓冲区溢出截断导致的越界
    if ((usize)len > sizeof(buf) - 1U) {
        len = (i32)(sizeof(buf) - 1U);
    }

    g_out_fn((const u8 *)buf, (usize)len);
    SLOG_LOCK_EXIT();
}

#if TEST_ENABLE
#include "em_test/test.h"

static char g_test_buf[256];
static usize g_test_len = 0;

/**
 * @brief 测试输出回调，捕获日志输出内容
 */
static void test_slog_output(const u8 *buf, usize len)
{
    g_test_len = len;
    if (g_test_len > sizeof(g_test_buf) - 1U) {
        g_test_len = sizeof(g_test_buf) - 1U;
    }
    if (g_test_len > 0U) {
        memcpy(g_test_buf, buf, g_test_len);
    }
    g_test_buf[g_test_len] = '\0';
}

/**
 * @brief 重置测试状态并初始化 logger
 */
static void test_slog_reset(void)
{
    g_test_len = 0U;
    g_test_buf[0] = '\0';
    slog_init(test_slog_output);
}

TEST_CASE(simple_logger_info_basic)
{
    test_slog_reset();
    log_info("value=%d", 7);
    TEST_ASSERT_TRUE(g_test_len > 0U);
    TEST_ASSERT_EQUAL_STRING("[INFO][] value=7\n", g_test_buf);
}

TEST_CASE(simple_logger_raw_no_newline)
{
    test_slog_reset();
    log_raw("abc");
    TEST_ASSERT_EQUAL_STRING("abc", g_test_buf);
}

TEST_CASE(simple_logger_format_core)
{
    test_slog_reset();
    _slog_printf("%d %u %x %X %s %.2f", -12, 42U, 0x2a, 0x2a, "ok", 3.14159);
    TEST_ASSERT_EQUAL_STRING("-12 42 2a 2A ok 3.14", g_test_buf);
}

TEST_CASE(simple_logger_format_unsigned_max)
{
    test_slog_reset();
    _slog_printf("%u", 4294967295U);
    TEST_ASSERT_EQUAL_STRING("4294967295", g_test_buf);
}

TEST_CASE(simple_logger_format_zero_pad_d)
{
    test_slog_reset();
    _slog_printf("%02d %03d %02d", 7, -7, 12);
    TEST_ASSERT_EQUAL_STRING("07 -07 12", g_test_buf);
}

TEST_CASE(simple_logger_format_zero_pad_u)
{
    test_slog_reset();
    _slog_printf("%02u %05u", 3U, 123U);
    TEST_ASSERT_EQUAL_STRING("03 00123", g_test_buf);
}

TEST_CASE(simple_logger_long_message_truncate)
{
    char long_str[320];
    usize i;

    for (i = 0; i < sizeof(long_str) - 1U; i++) {
        long_str[i] = 'A';
    }
    long_str[sizeof(long_str) - 1U] = '\0';

    test_slog_reset();
    log_raw("%s", long_str);

    TEST_ASSERT_TRUE(g_test_len <= (usize)(SLOG_BUFFER_SIZE - 1));
    TEST_ASSERT_TRUE(g_test_buf[g_test_len] == '\0');
}

#if SLOG_ENABLE_RUNTIME_LEVEL
TEST_CASE(simple_logger_runtime_filter)
{
    test_slog_reset();
    slog_set_runtime_level(LOG_LEVEL_WARN);
    log_info("drop_me");
    TEST_ASSERT_EQUAL_UINT(0, (u32)g_test_len);

    log_error("keep_me");
    TEST_ASSERT_TRUE(g_test_len > 0U);
    TEST_ASSERT_NOT_NULL(strstr(g_test_buf, "keep_me"));

    slog_set_runtime_level(LOG_LEVEL_DEBUG);
}
#endif

#endif
