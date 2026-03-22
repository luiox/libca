/* Auto-migrated from src/em_log/simple_logger.c test blocks */
#include "simple_logger.h"
#include <em_format/format.h>
#include <stdarg.h>
#include <stdio.h> // 仅用于 vsnprintf

#include <string.h>
#include <em_test/test.h>

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

