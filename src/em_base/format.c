#include "format.h"

#define FMT_SPRINTF_MAX ((usize)(~(usize)0))

static void safe_buf_putc(char *buf, usize buf_len, usize *pos, char ch)
{
    if (buf_len > 0U && *pos + 1U < buf_len) {
        buf[*pos] = ch;
    }
    *pos = *pos + 1U;
}

static void safe_buf_terminate(char *buf, usize buf_len, usize pos)
{
    if (buf == NULL || buf_len == 0U) {
        return;
    }

    if (pos >= buf_len) {
        buf[buf_len - 1U] = '\0';
        return;
    }

    buf[pos] = '\0';
}

static void fmt_buf_putc(char *buf, usize buf_size, usize *pos, char ch)
{
    safe_buf_putc(buf, buf_size, pos, ch);
}

static void fmt_buf_puts(char *buf, usize buf_size, usize *pos, const char *str)
{
    if (str == NULL) {
        str = "(null)";
    }

    while (*str != '\0') {
        fmt_buf_putc(buf, buf_size, pos, *str);
        str++;
    }
}

static void fmt_buf_put_u32_base(char *buf, usize buf_size, usize *pos, u32 value, u32 base, bool upper)
{
    char digits_lower[] = "0123456789abcdef";
    char digits_upper[] = "0123456789ABCDEF";
    char tmp[16];
    usize i = 0U;

    if (value == 0U) {
        fmt_buf_putc(buf, buf_size, pos, '0');
        return;
    }

    while (value != 0U && i < (usize)sizeof(tmp)) {
        u32 d = value % base;
        tmp[i++] = upper ? digits_upper[d] : digits_lower[d];
        value /= base;
    }

    while (i > 0U) {
        i--;
        fmt_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

static void fmt_buf_put_u32_dec(char *buf, usize buf_size, usize *pos, u32 value)
{
    char tmp[16];
    usize len = u32_to_str_safe(tmp, sizeof(tmp), value);
    usize i;

    for (i = 0U; i < len; i++) {
        fmt_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

static void fmt_buf_put_u32_width(char *buf, usize buf_size, usize *pos, u32 value, u32 min_width, char pad_char)
{
    char tmp[16];
    usize digits_len = u32_to_str_safe(tmp, sizeof(tmp), value);
    usize pad_count;
    usize i;

    pad_count = min_width > digits_len ? (min_width - digits_len) : 0U;
    for (i = 0U; i < pad_count; i++) {
        fmt_buf_putc(buf, buf_size, pos, pad_char);
    }

    for (i = 0U; i < digits_len; i++) {
        fmt_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

static void fmt_buf_put_i32_width(char *buf, usize buf_size, usize *pos, i32 value, u32 min_width, char pad_char)
{
    bool negative = false;
    u32 abs_value;
    usize digits_len;
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

    digits_len = u32_to_str_safe(tmp, sizeof(tmp), abs_value);

    total_len = digits_len + (negative ? 1U : 0U);
    pad_count = min_width > total_len ? (min_width - total_len) : 0U;

    if (pad_char == '0') {
        if (negative) {
            fmt_buf_putc(buf, buf_size, pos, '-');
        }
        for (i = 0U; i < pad_count; i++) {
            fmt_buf_putc(buf, buf_size, pos, '0');
        }
    }
    else {
        for (i = 0U; i < pad_count; i++) {
            fmt_buf_putc(buf, buf_size, pos, pad_char);
        }
        if (negative) {
            fmt_buf_putc(buf, buf_size, pos, '-');
        }
    }

    for (i = 0U; i < digits_len; i++) {
        fmt_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

static void fmt_buf_put_i32(char *buf, usize buf_size, usize *pos, i32 value)
{
    fmt_buf_put_i32_width(buf, buf_size, pos, value, 0U, ' ');
}

static void fmt_buf_put_f64_trunc(char *buf, usize buf_size, usize *pos, f64 value, u32 precision)
{
    char tmp[48];
    usize len = f32_to_str_safe(tmp, sizeof(tmp), (float)value, precision);
    usize i;

    for (i = 0U; i < len; i++) {
        fmt_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

/**
 * @brief 将 u32 转为十进制字符串（无边界检查）
 */
usize u32_to_str(char *buf, u32 val)
{
    char tmp[16];
    usize i = 0U;
    usize len;

    if (buf == NULL) {
        return 0U;
    }

    if (val == 0U) {
        buf[0] = '0';
        return 1U;
    }

    while (val > 0U && i < (usize)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (val % 10U));
        val /= 10U;
    }

    len = i;
    while (i > 0U) {
        i--;
        *buf++ = tmp[i];
    }

    return len;
}

/**
 * @brief 将 u32 转为十进制字符串（安全版，保证 '\0' 结尾）
 */
usize u32_to_str_safe(char *buf, usize buf_len, u32 val)
{
    char tmp[16];
    usize total_len;
    usize copy_len;
    usize i;

    if (buf == NULL || buf_len == 0U) {
        return 0U;
    }

    total_len = u32_to_str(tmp, val);
    copy_len = total_len;
    if (copy_len + 1U > buf_len) {
        copy_len = buf_len - 1U;
    }

    for (i = 0U; i < copy_len; i++) {
        buf[i] = tmp[i];
    }
    buf[copy_len] = '\0';

    return copy_len;
}

/**
 * @brief 将 f32 转为定点十进制字符串（无边界检查）
 */
usize f32_to_str(char *buf, f32 val, u32 decimal_num)
{
    char *p = buf;
    f32 abs_val = val;
    u32 int_part;
    f32 frac_val;
    u32 i;
    usize int_len;

    if (buf == NULL) {
        return 0U;
    }

    if (abs_val < 0.0f) {
        *p++ = '-';
        abs_val = -abs_val;
    }

    int_part = (u32)abs_val;
    frac_val = abs_val - (float)int_part;
    int_len = u32_to_str(p, int_part);
    p += int_len;

    if (decimal_num > 0U) {
        *p++ = '.';
        for (i = 0U; i < decimal_num; i++) {
            u32 digit;
            frac_val *= 10.0f;
            digit = (u32)frac_val;
            if (digit > 9U) {
                digit = 9U;
            }
            *p++ = (char)('0' + digit);
            frac_val -= (float)digit;
        }
    }

    *p = '\0';
    return (usize)(p - buf);
}

/**
 * @brief 将 f32 转为定点十进制字符串（安全版，保证 '\0' 结尾）
 */
usize f32_to_str_safe(char *buf, usize buf_len, f32 val, u32 decimal_num)
{
    usize pos = 0U;
    f32 abs_val = val;
    u32 int_part;
    f32 frac_val;
    char int_buf[16];
    usize int_len;
    usize i;

    if (buf == NULL || buf_len == 0U) {
        return 0U;
    }

    if (abs_val < 0.0f) {
        safe_buf_putc(buf, buf_len, &pos, '-');
        abs_val = -abs_val;
    }

    int_part = (u32)abs_val;
    frac_val = abs_val - (f32)int_part;

    int_len = u32_to_str_safe(int_buf, sizeof(int_buf), int_part);
    for (i = 0U; i < int_len; i++) {
        safe_buf_putc(buf, buf_len, &pos, int_buf[i]);
    }

    if (decimal_num > 0U) {
        safe_buf_putc(buf, buf_len, &pos, '.');
        for (i = 0U; i < decimal_num; i++) {
            u32 digit;
            frac_val *= 10.0f;
            digit = (u32)frac_val;
            if (digit > 9U) {
                digit = 9U;
            }
            safe_buf_putc(buf, buf_len, &pos, (char)('0' + digit));
            frac_val -= (f32)digit;
        }
    }

    safe_buf_terminate(buf, buf_len, pos);
    if (pos >= buf_len) {
        return buf_len - 1U;
    }
    return pos;
}

/**
 * @brief 轻量格式化（va_list 版本）
 */
i32 fmt_vsnprintf(char *buf, usize buf_size, const char *fmt, va_list args)
{
    usize pos = 0U;

    if (buf == NULL || fmt == NULL || buf_size == 0U) {
        return 0;
    }

    while (*fmt != '\0') {
        if (*fmt != '%') {
            fmt_buf_putc(buf, buf_size, &pos, *fmt);
            fmt++;
            continue;
        }

        fmt++;
        if (*fmt == '%') {
            fmt_buf_putc(buf, buf_size, &pos, '%');
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
                fmt_buf_put_i32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                fmt_buf_put_i32(buf, buf_size, &pos, value);
            }
            break;
        }
        case 'u': {
            u32 value = (u32)va_arg(args, unsigned int);
            if (width > 0U) {
                fmt_buf_put_u32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                fmt_buf_put_u32_dec(buf, buf_size, &pos, value);
            }
            break;
        }
        case 'x': {
            u32 value = (u32)va_arg(args, unsigned int);
            fmt_buf_put_u32_base(buf, buf_size, &pos, value, 16U, false);
            break;
        }
        case 'X': {
            u32 value = (u32)va_arg(args, unsigned int);
            fmt_buf_put_u32_base(buf, buf_size, &pos, value, 16U, true);
            break;
        }
        case 's': {
            const char *value = va_arg(args, const char *);
            fmt_buf_puts(buf, buf_size, &pos, value);
            break;
        }
        case 'f': {
            f64 value = (f64)va_arg(args, double);
            fmt_buf_put_f64_trunc(buf, buf_size, &pos, value, precision);
            break;
        }
        case '\0':
            fmt--;
            break;
        default:
            fmt_buf_putc(buf, buf_size, &pos, '%');
            if (has_precision) {
                fmt_buf_putc(buf, buf_size, &pos, '.');
            }
            if (*fmt != '\0') {
                fmt_buf_putc(buf, buf_size, &pos, *fmt);
            }
            break;
        }

        if (*fmt != '\0') {
            fmt++;
        }
    }

    if (pos >= buf_size) {
        buf[buf_size - 1U] = '\0';
        return (i32)(buf_size - 1U);
    }

    buf[pos] = '\0';
    return (i32)pos;
}

/**
 * @brief 轻量格式化（有界版本）
 */
i32 fmt_snprintf(char *buf, usize buf_size, const char *fmt, ...)
{
    va_list args;
    i32 len;

    if (buf == NULL || fmt == NULL || buf_size == 0U) {
        return 0;
    }

    va_start(args, fmt);
    len = fmt_vsnprintf(buf, buf_size, fmt, args);
    va_end(args);

    return len;
}

/**
 * @brief 轻量格式化（无界版本）
 */
i32 fmt_sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    i32 len;

    if (buf == NULL || fmt == NULL) {
        return 0;
    }

    va_start(args, fmt);
    len = fmt_vsnprintf(buf, FMT_SPRINTF_MAX, fmt, args);
    va_end(args);

    return len;
}


#if TEST_ENABLE
#include "../em_test/test.h"

static i32 test_fmt_vsnprintf_call(char *buf, usize buf_size, const char *fmt, ...)
{
    va_list args;
    i32 len;

    va_start(args, fmt);
    len = fmt_vsnprintf(buf, buf_size, fmt, args);
    va_end(args);

    return len;
}

TEST_CASE(test_u32_to_str_basic)
{
    char buf[16];
    usize len;

    len = u32_to_str(buf, 0U);
    buf[len] = '\0';
    TEST_EXPECT_EQ_U32(1U, (u32)len);
    TEST_EXPECT_EQ_STR("0", buf);

    len = u32_to_str(buf, 4294967295U);
    buf[len] = '\0';
    TEST_EXPECT_EQ_U32(10U, (u32)len);
    TEST_EXPECT_EQ_STR("4294967295", buf);

    TEST_EXPECT_EQ_U32(0U, (u32)u32_to_str(NULL, 7U));
}

TEST_CASE(test_u32_to_str_safe)
{
    char buf[8];
    char guard[10] = {'L', 'L', '#', '#', '#', '#', '#', '#', 'R', 'R'};
    usize len;

    len = u32_to_str_safe(buf, sizeof(buf), 12345U);
    TEST_EXPECT_EQ_U32(5U, (u32)len);
    TEST_EXPECT_EQ_STR("12345", buf);

    len = u32_to_str_safe(buf, 4U, 12345U);
    TEST_EXPECT_EQ_U32(3U, (u32)len);
    TEST_EXPECT_EQ_STR("123", buf);

    len = u32_to_str_safe(&guard[2], 4U, 12345U);
    TEST_EXPECT_EQ_U32(3U, (u32)len);
    TEST_EXPECT_EQ_STR("123", &guard[2]);
    TEST_EXPECT_EQ_I8('L', guard[0]);
    TEST_EXPECT_EQ_I8('L', guard[1]);
    TEST_EXPECT_EQ_I8('R', guard[8]);
    TEST_EXPECT_EQ_I8('R', guard[9]);

    len = u32_to_str_safe(buf, 1U, 88U);
    TEST_EXPECT_EQ_U32(0U, (u32)len);
    TEST_EXPECT_EQ_STR("", buf);

    TEST_EXPECT_EQ_U32(0U, (u32)u32_to_str_safe(NULL, 4U, 7U));
    TEST_EXPECT_EQ_U32(0U, (u32)u32_to_str_safe(buf, 0U, 7U));
}

TEST_CASE(test_f32_to_str_basic)
{
    char buf[32];
    usize len;

    len = f32_to_str(buf, 3.14159f, 2U);
    TEST_EXPECT_EQ_U32(4U, (u32)len);
    TEST_EXPECT_EQ_STR("3.14", buf);

    len = f32_to_str(buf, -0.5f, 3U);
    TEST_EXPECT_EQ_U32(6U, (u32)len);
    TEST_EXPECT_EQ_STR("-0.500", buf);

    len = f32_to_str(buf, 42.9f, 0U);
    TEST_EXPECT_EQ_U32(2U, (u32)len);
    TEST_EXPECT_EQ_STR("42", buf);

    TEST_EXPECT_EQ_U32(0U, (u32)f32_to_str(NULL, 1.25f, 2U));
}

TEST_CASE(test_f32_to_str_safe)
{
    char buf[16];
    char guard[12] = {'L', '#', '#', '#', '#', '#', '#', '#', '#', '#', '#', 'R'};
    usize len;

    len = f32_to_str_safe(buf, sizeof(buf), 1.25f, 3U);
    TEST_EXPECT_EQ_U32(5U, (u32)len);
    TEST_EXPECT_EQ_STR("1.250", buf);

    len = f32_to_str_safe(buf, 5U, 123.45f, 2U);
    TEST_EXPECT_EQ_U32(4U, (u32)len);
    TEST_EXPECT_EQ_STR("123.", buf);

    len = f32_to_str_safe(&guard[1], 4U, -9.99f, 2U);
    TEST_EXPECT_EQ_U32(3U, (u32)len);
    TEST_EXPECT_EQ_STR("-9.", &guard[1]);
    TEST_EXPECT_EQ_I8('L', guard[0]);
    TEST_EXPECT_EQ_I8('R', guard[11]);

    len = f32_to_str_safe(buf, 1U, -3.14f, 2U);
    TEST_EXPECT_EQ_U32(0U, (u32)len);
    TEST_EXPECT_EQ_STR("", buf);

    TEST_EXPECT_EQ_U32(0U, (u32)f32_to_str_safe(NULL, 8U, 1.0f, 2U));
    TEST_EXPECT_EQ_U32(0U, (u32)f32_to_str_safe(buf, 0U, 1.0f, 2U));
}

TEST_CASE(test_fmt_sprintf_core)
{
    char buf[96];
    i32 len;

    len = fmt_sprintf(buf, "%d %u %x %X %s %.2f %%", -12, 42U, 0x2aU, 0x2aU, "ok", 3.14159);
    TEST_EXPECT_EQ_I32(22, len);
    TEST_EXPECT_EQ_STR("-12 42 2a 2A ok 3.14 %", buf);

    len = fmt_sprintf(buf, "%s", NULL);
    TEST_EXPECT_EQ_I32(6, len);
    TEST_EXPECT_EQ_STR("(null)", buf);
}

TEST_CASE(test_fmt_snprintf_basic_and_truncate)
{
    char buf[10];
    i32 len;

    len = fmt_snprintf(buf, sizeof(buf), "%s-%u", "ab", 12U);
    TEST_EXPECT_EQ_I32(5, len);
    TEST_EXPECT_EQ_STR("ab-12", buf);

    len = fmt_snprintf(buf, 6U, "%s", "123456789");
    TEST_EXPECT_EQ_I32(5, len);
    TEST_EXPECT_EQ_STR("12345", buf);

    len = fmt_snprintf(buf, 1U, "xyz");
    TEST_EXPECT_EQ_I32(0, len);
    TEST_EXPECT_EQ_STR("", buf);

    TEST_EXPECT_EQ_I32(0, fmt_snprintf(NULL, sizeof(buf), "x"));
    TEST_EXPECT_EQ_I32(0, fmt_snprintf(buf, sizeof(buf), NULL));
    TEST_EXPECT_EQ_I32(0, fmt_snprintf(buf, 0U, "x"));
}

TEST_CASE(test_fmt_padding_and_unknown_spec)
{
    char buf[96];

    TEST_EXPECT_EQ_I32(9, test_fmt_vsnprintf_call(buf, sizeof(buf), "%02d %03d %02u", 7, -7, 3U));
    TEST_EXPECT_EQ_STR("07 -07 03", buf);

    TEST_EXPECT_EQ_I32(3, test_fmt_vsnprintf_call(buf, sizeof(buf), "%.2q"));
    TEST_EXPECT_EQ_STR("%.q", buf);

    TEST_EXPECT_EQ_I32(3, test_fmt_vsnprintf_call(buf, sizeof(buf), "abc%"));
    TEST_EXPECT_EQ_STR("abc", buf);
}

TEST_CASE(test_fmt_buffer_boundary)
{
    char buf[8];
    i32 len;

    len = test_fmt_vsnprintf_call(buf, sizeof(buf), "123456789");
    TEST_EXPECT_EQ_I32(7, len);
    TEST_EXPECT_EQ_STR("1234567", buf);

    len = test_fmt_vsnprintf_call(buf, 1U, "A");
    TEST_EXPECT_EQ_I32(0, len);
    TEST_EXPECT_EQ_STR("", buf);
}

TEST_CASE(test_fmt_invalid_input)
{
    char buf[8];

    TEST_EXPECT_EQ_I32(0, fmt_sprintf(NULL, "x"));
    TEST_EXPECT_EQ_I32(0, fmt_sprintf(buf, NULL));

    TEST_EXPECT_EQ_I32(0, test_fmt_vsnprintf_call(NULL, sizeof(buf), "x"));
    TEST_EXPECT_EQ_I32(0, test_fmt_vsnprintf_call(buf, 0U, "x"));
}

#endif

