#include "simple_logger.h"
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
 * @brief 向输出缓冲区追加一个字符
 */
static void slog_buf_putc(char *buf, usize buf_size, usize *pos, char ch)
{
    if (buf_size > 0 && *pos + 1 < buf_size) {
        buf[*pos] = ch;
    }
    *pos = *pos + 1;
}

/**
 * @brief 向输出缓冲区追加字符串
 */
static void slog_buf_puts(char *buf, usize buf_size, usize *pos, const char *str)
{
    if (str == NULL) {
        str = "(null)";
    }

    while (*str != '\0') {
        slog_buf_putc(buf, buf_size, pos, *str);
        str++;
    }
}

/**
 * @brief 以指定进制追加无符号整数
 */
static void slog_buf_put_u32_base(char *buf, usize buf_size, usize *pos, u32 value, u32 base, bool upper)
{
    char digits_lower[] = "0123456789abcdef";
    char digits_upper[] = "0123456789ABCDEF";
    char tmp[16];
    usize i = 0;

    if (value == 0) {
        slog_buf_putc(buf, buf_size, pos, '0');
        return;
    }

    while (value != 0 && i < (usize)sizeof(tmp)) {
        u32 d = value % base;
        tmp[i++] = upper ? digits_upper[d] : digits_lower[d];
        value /= base;
    }

    while (i > 0) {
        i--;
        slog_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

/**
 * @brief 按最小宽度追加无符号32位十进制整数
 */
static void slog_buf_put_u32_width(char *buf, usize buf_size, usize *pos, u32 value, u32 min_width, char pad_char)
{
    char tmp[16];
    usize digits_len = 0;
    usize pad_count;
    usize i;

    if (value == 0U) {
        tmp[digits_len++] = '0';
    }
    else {
        while (value != 0U && digits_len < (usize)sizeof(tmp)) {
            u32 d = value % 10U;
            tmp[digits_len++] = (char)('0' + d);
            value /= 10U;
        }
    }

    pad_count = min_width > digits_len ? (min_width - digits_len) : 0U;

    for (i = 0; i < pad_count; i++) {
        slog_buf_putc(buf, buf_size, pos, pad_char);
    }

    while (digits_len > 0U) {
        digits_len--;
        slog_buf_putc(buf, buf_size, pos, tmp[digits_len]);
    }
}

/**
 * @brief 按最小宽度追加有符号32位十进制整数
 */
static void slog_buf_put_i32_width(char *buf, usize buf_size, usize *pos, i32 value, u32 min_width, char pad_char)
{
    bool negative = false;
    u32 abs_value;
    usize digits_len = 0;
    usize total_len;
    usize pad_count;
    char tmp[16];
    usize i;

    if (value < 0) {
        negative = true;
        abs_value = (u32)(-(value + 1)) + 1U;
    }
    else {
        abs_value = (u32)value;
    }

    if (abs_value == 0U) {
        tmp[digits_len++] = '0';
    }
    else {
        while (abs_value != 0U && digits_len < (usize)sizeof(tmp)) {
            u32 d = abs_value % 10U;
            tmp[digits_len++] = (char)('0' + d);
            abs_value /= 10U;
        }
    }

    total_len = digits_len + (negative ? 1U : 0U);
    pad_count = min_width > total_len ? (min_width - total_len) : 0U;

    if (pad_char == '0') {
        if (negative) {
            slog_buf_putc(buf, buf_size, pos, '-');
        }
        for (i = 0; i < pad_count; i++) {
            slog_buf_putc(buf, buf_size, pos, '0');
        }
    }
    else {
        for (i = 0; i < pad_count; i++) {
            slog_buf_putc(buf, buf_size, pos, pad_char);
        }
        if (negative) {
            slog_buf_putc(buf, buf_size, pos, '-');
        }
    }

    while (digits_len > 0U) {
        digits_len--;
        slog_buf_putc(buf, buf_size, pos, tmp[digits_len]);
    }
}

/**
 * @brief 追加有符号32位整数
 */
static void slog_buf_put_i32(char *buf, usize buf_size, usize *pos, i32 value)
{
    slog_buf_put_i32_width(buf, buf_size, pos, value, 0U, ' ');
}

/**
 * @brief 追加截断策略的浮点数（固定小数位）
 */
static void slog_buf_put_f64_trunc(char *buf, usize buf_size, usize *pos, f64 value, u32 precision)
{
    f64 abs_value = value;
    u32 int_part;
    f64 frac_part;

    if (abs_value < 0.0) {
        slog_buf_putc(buf, buf_size, pos, '-');
        abs_value = -abs_value;
    }

    int_part = (u32)abs_value;
    frac_part = abs_value - (f64)int_part;

    slog_buf_put_u32_base(buf, buf_size, pos, int_part, 10U, false);

    if (precision == 0U) {
        return;
    }

    slog_buf_putc(buf, buf_size, pos, '.');
    while (precision > 0U) {
        u32 digit;
        frac_part *= 10.0;
        digit = (u32)frac_part;
        slog_buf_putc(buf, buf_size, pos, (char)('0' + digit));
        frac_part -= (f64)digit;
        precision--;
    }
}

/**
 * @brief 轻量级格式化函数，仅支持 %d %u %f %x %X %s 和 %%，支持 %.Nf 与 %0Nd
 */
static i32 slog_fast_vsnprintf(char *buf, usize buf_size, const char *fmt, va_list args)
{
    usize pos = 0;

    if (buf == NULL || fmt == NULL || buf_size == 0U) {
        return 0;
    }

    while (*fmt != '\0') {
        if (*fmt != '%') {
            slog_buf_putc(buf, buf_size, &pos, *fmt);
            fmt++;
            continue;
        }

        fmt++;
        if (*fmt == '%') {
            slog_buf_putc(buf, buf_size, &pos, '%');
            fmt++;
            continue;
        }

        bool zero_pad = false;
        u32 width = 0U;

        u32 precision = 6U;
        bool has_precision = false;

        if (*fmt == '0') {
            zero_pad = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10U + (u32)(*fmt - '0');
            fmt++;
        }

        if (*fmt == '.') {
            has_precision = true;
            precision = 0U;
            fmt++;
            while (*fmt >= '0' && *fmt <= '9') {
                precision = precision * 10U + (u32)(*fmt - '0');
                fmt++;
            }
        }

        switch (*fmt) {
        case 'd': {
            i32 value = (i32)va_arg(args, int);
            if (width > 0U) {
                slog_buf_put_i32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                slog_buf_put_i32(buf, buf_size, &pos, value);
            }
            break;
        }
        case 'u': {
            u32 value = (u32)va_arg(args, unsigned int);
            if (width > 0U) {
                slog_buf_put_u32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                slog_buf_put_u32_base(buf, buf_size, &pos, value, 10U, false);
            }
            break;
        }
        case 'x': {
            u32 value = (u32)va_arg(args, unsigned int);
            slog_buf_put_u32_base(buf, buf_size, &pos, value, 16U, false);
            break;
        }
        case 'X': {
            u32 value = (u32)va_arg(args, unsigned int);
            slog_buf_put_u32_base(buf, buf_size, &pos, value, 16U, true);
            break;
        }
        case 's': {
            const char *value = va_arg(args, const char *);
            slog_buf_puts(buf, buf_size, &pos, value);
            break;
        }
        case 'f': {
            f64 value = (f64)va_arg(args, double);
            slog_buf_put_f64_trunc(buf, buf_size, &pos, value, precision);
            break;
        }
        case '\0':
            fmt--;
            break;
        default:
            slog_buf_putc(buf, buf_size, &pos, '%');
            if (has_precision) {
                slog_buf_putc(buf, buf_size, &pos, '.');
            }
            if (*fmt != '\0') {
                slog_buf_putc(buf, buf_size, &pos, *fmt);
            }
            break;
        }

        if (*fmt != '\0') {
            fmt++;
        }
    }

    if (pos >= buf_size) {
        buf[buf_size - 1] = '\0';
        return (i32)(buf_size - 1U);
    }

    buf[pos] = '\0';
    return (i32)pos;
}

/**
 * @brief 统一格式化入口，可在标准 vsnprintf 与 fast_vsnprintf 之间切换
 */
static i32 slog_vsnprintf(char *buf, usize buf_size, const char *fmt, va_list args)
{
#if SLOG_USE_FAST_VSNPRINTF
    return slog_fast_vsnprintf(buf, buf_size, fmt, args);
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
#include "../em_test/test.h"

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
