#include "format.h"

#define FMT_SPRINTF_MAX ((usize)(~(usize)0))

static void safe_buf_putc(char* buf, usize buf_len, usize* pos, char ch)
{
    if (buf_len > 0U && *pos + 1U < buf_len) {
        buf[*pos] = ch;
    }
    *pos = *pos + 1U;
}

static void safe_buf_terminate(char* buf, usize buf_len, usize pos)
{
    if (buf == NULL || buf_len == 0U) {
        return;
    }

    if (pos >= buf_len) {
        buf[buf_len - 1U] = '\0';
    }
    else {
        buf[pos] = '\0';
    }
}

usize u32_to_str(char* buf, u32 val)
{
    char  tmp[FMT_U32_TMP_BUF_SIZE];
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

usize u32_to_str_safe(char* buf, usize buf_len, u32 val)
{
    char  tmp[FMT_U32_TMP_BUF_SIZE];
    usize total_len;
    usize copy_len;
    usize i;

    if (buf == NULL || buf_len == 0U) {
        return 0U;
    }

    total_len = u32_to_str(tmp, val);
    copy_len  = total_len;
    if (copy_len + 1U > buf_len) {
        copy_len = buf_len - 1U;
    }

    for (i = 0U; i < copy_len; i++) {
        buf[i] = tmp[i];
    }
    buf[copy_len] = '\0';

    return copy_len;
}

#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
static f64 fmt_pow10_neg(u32 n)
{
    static const f64 pow10_neg_tbl[] = {1.0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8};
    if (n < sizeof(pow10_neg_tbl) / sizeof(pow10_neg_tbl[0])) {
        return pow10_neg_tbl[n];
    }

    f64 value = pow10_neg_tbl[(sizeof(pow10_neg_tbl) / sizeof(pow10_neg_tbl[0])) - 1U];
    n -= (u32)((sizeof(pow10_neg_tbl) / sizeof(pow10_neg_tbl[0])) - 1U);
    while (n > 0U) {
        value /= 10.0;
        n--;
    }
    return value;
}

static u32 fmt_next_frac_digit(f64* frac_val)
{
    f64 scaled = (*frac_val) * 10.0;
    u32 digit;

    if (scaled < 0.0) {
        scaled = 0.0;
    }
    else if (scaled >= 10.0) {
        scaled = 9.999999999;
    }

    digit = (u32)scaled;
    *frac_val = scaled - (f64)digit;
    if (*frac_val < 0.0) {
        *frac_val = 0.0;
    }

    return digit;
}

static usize f64_to_str_core(char* buf, usize buf_len, f64 val, u32 decimal_num, bool round_mode)
{
    usize pos     = 0U;
    f64   abs_val = val;
    u32   int_part;
    f64   frac_val;
    char  int_buf[FMT_U32_TMP_BUF_SIZE];
    usize int_len;
    usize i;

    if (buf == NULL || buf_len == 0U) {
        return 0U;
    }

    if (abs_val < 0.0) {
        safe_buf_putc(buf, buf_len, &pos, '-');
        abs_val = -abs_val;
    }

    if (round_mode && decimal_num > 0U) {
        abs_val += 0.5 * fmt_pow10_neg(decimal_num);
    }

    if (abs_val >= 4294967295.0) {
        int_part = 4294967295U;
        frac_val = 0.0;
    }
    else {
        int_part = (u32)abs_val;
        frac_val = abs_val - (f64)int_part;
        if (frac_val < 0.0) {
            frac_val = 0.0;
        }
        if (frac_val >= 1.0) {
            frac_val = 0.0;
            if (int_part < 4294967295U) {
                int_part++;
            }
        }
    }

    int_len = u32_to_str_safe(int_buf, sizeof(int_buf), int_part);
    for (i = 0U; i < int_len; i++) {
        safe_buf_putc(buf, buf_len, &pos, int_buf[i]);
    }

    if (decimal_num > 0U) {
        safe_buf_putc(buf, buf_len, &pos, '.');
        for (i = 0U; i < decimal_num; i++) {
            u32 digit = fmt_next_frac_digit(&frac_val);
            safe_buf_putc(buf, buf_len, &pos, (char)('0' + digit));
        }
    }

    safe_buf_terminate(buf, buf_len, pos);
    if (pos >= buf_len) {
        return buf_len - 1U;
    }
    return pos;
}
#else
static u32 fmt_pow10_u32(u32 n)
{
    static const u32 pow10_tbl[] = {
        1U, 10U, 100U, 1000U, 10000U, 100000U, 1000000U, 10000000U, 100000000U, 1000000000U};
    if (n < (u32)(sizeof(pow10_tbl) / sizeof(pow10_tbl[0]))) {
        return pow10_tbl[n];
    }
    return 1000000000U;
}

static usize f64_to_str_core(char* buf, usize buf_len, f64 val, u32 decimal_num, bool round_mode)
{
    usize pos     = 0U;
    f64   abs_val = val;
    u32   int_part;
    f64   frac_val;
    u32   frac_part = 0U;
    u32   pow10;
    char  int_buf[FMT_U32_TMP_BUF_SIZE];
    usize int_len;
    usize i;

    (void)round_mode;

    if (buf == NULL || buf_len == 0U) {
        return 0U;
    }

    if (abs_val < 0.0) {
        safe_buf_putc(buf, buf_len, &pos, '-');
        abs_val = -abs_val;
    }

    int_part = (u32)abs_val;
    frac_val = abs_val - (f64)int_part;
    if (frac_val < 0.0) {
        frac_val = 0.0;
    }

    int_len = u32_to_str_safe(int_buf, sizeof(int_buf), int_part);
    for (i = 0U; i < int_len; i++) {
        safe_buf_putc(buf, buf_len, &pos, int_buf[i]);
    }

    if (decimal_num > 0U) {
        safe_buf_putc(buf, buf_len, &pos, '.');

        pow10 = fmt_pow10_u32(decimal_num);
        frac_part = (u32)(frac_val * (f64)pow10);

        if (pow10 > 1U) {
            u32 div = pow10 / 10U;
            while (div > 0U) {
                u32 digit = frac_part / div;
                safe_buf_putc(buf, buf_len, &pos, (char)('0' + (digit % 10U)));
                frac_part %= div;
                div /= 10U;
            }
        }
        else {
            safe_buf_putc(buf, buf_len, &pos, (char)('0' + (frac_part % 10U)));
        }
    }

    safe_buf_terminate(buf, buf_len, pos);
    if (pos >= buf_len) {
        return buf_len - 1U;
    }
    return pos;
}
#endif

usize f32_to_str(char* buf, f32 val, u32 decimal_num)
{
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    decimal_num = FMT_FIXED_DECIMALS;
#endif
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    return f64_to_str_core(buf, FMT_SPRINTF_MAX, (f64)val, decimal_num, true);
#else
    return f64_to_str_core(buf, FMT_SPRINTF_MAX, (f64)val, decimal_num, false);
#endif
}

usize f32_to_str_safe(char* buf, usize buf_len, f32 val, u32 decimal_num)
{
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    decimal_num = FMT_FIXED_DECIMALS;
#endif
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    return f64_to_str_core(buf, buf_len, (f64)val, decimal_num, true);
#else
    return f64_to_str_core(buf, buf_len, (f64)val, decimal_num, false);
#endif
}

usize f64_to_str(char* buf, f64 val, u32 decimal_num)
{
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    decimal_num = FMT_FIXED_DECIMALS;
#endif
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    return f64_to_str_core(buf, FMT_SPRINTF_MAX, val, decimal_num, true);
#else
    return f64_to_str_core(buf, FMT_SPRINTF_MAX, (f64)(f32)val, decimal_num, false);
#endif
}

usize f64_to_str_safe(char* buf, usize buf_len, f64 val, u32 decimal_num)
{
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    decimal_num = FMT_FIXED_DECIMALS;
#endif
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    return f64_to_str_core(buf, buf_len, val, decimal_num, true);
#else
    return f64_to_str_core(buf, buf_len, (f64)(f32)val, decimal_num, false);
#endif
}

static void fmt_buf_puts(char* buf, usize buf_size, usize* pos, const char* str)
{
    if (str == NULL) {
        str = "(null)";
    }

    while (*str != '\0') {
        safe_buf_putc(buf, buf_size, pos, *str);
        str++;
    }
}

static void fmt_buf_put_u32_dec(char* buf, usize buf_size, usize* pos, u32 value)
{
    char  tmp[FMT_U32_TMP_BUF_SIZE];
    usize len = u32_to_str_safe(tmp, sizeof(tmp), value);
    usize i;

    for (i = 0U; i < len; i++) {
        safe_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

#if FMT_ENABLE_HEX
static void fmt_buf_put_u32_hex(char* buf, usize buf_size, usize* pos, u32 value, bool upper)
{
    char  digits_lower[] = "0123456789abcdef";
    char  digits_upper[] = "0123456789ABCDEF";
    char  tmp[FMT_BASE_CONV_TMP_BUF_SIZE];
    usize i = 0U;

    if (value == 0U) {
        safe_buf_putc(buf, buf_size, pos, '0');
        return;
    }

    while (value != 0U && i < (usize)sizeof(tmp)) {
        // 明确写出以提高优化性能
        u32 d = value & 0xFU; // u32 d = value % 16U;
        tmp[i++] = upper ? digits_upper[d] : digits_lower[d];
        value >>= 4; // value /= 16U;
    }

    while (i > 0U) {
        i--;
        safe_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}
#endif

#if FMT_ENABLE_WIDTH_PRECISION
static void fmt_buf_put_u32_width(char* buf, usize buf_size, usize* pos, u32 value, u32 min_width,
                                  char pad_char)
{
    char  tmp[FMT_U32_TMP_BUF_SIZE];
    usize digits_len = u32_to_str_safe(tmp, sizeof(tmp), value);
    usize pad_count;
    usize i;

    pad_count = min_width > digits_len ? (min_width - digits_len) : 0U;
    for (i = 0U; i < pad_count; i++) {
        safe_buf_putc(buf, buf_size, pos, pad_char);
    }

    for (i = 0U; i < digits_len; i++) {
        safe_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

static void fmt_buf_put_i32_width(char* buf, usize buf_size, usize* pos, i32 value, u32 min_width,
                                  char pad_char)
{
    bool  negative = false;
    u32   abs_value;
    usize digits_len;
    usize total_len;
    usize pad_count;
    char  tmp[FMT_U32_TMP_BUF_SIZE];
    usize i;

    if (value < 0) {
        negative  = true;
        abs_value = (u32)(-(value + 1)) + 1U;
    }
    else {
        abs_value = (u32)value;
    }

    digits_len = u32_to_str_safe(tmp, sizeof(tmp), abs_value);
    total_len  = digits_len + (negative ? 1U : 0U);
    pad_count  = min_width > total_len ? (min_width - total_len) : 0U;

    if (pad_char == '0') {
        if (negative) {
            safe_buf_putc(buf, buf_size, pos, '-');
        }
        for (i = 0U; i < pad_count; i++) {
            safe_buf_putc(buf, buf_size, pos, '0');
        }
    }
    else {
        for (i = 0U; i < pad_count; i++) {
            safe_buf_putc(buf, buf_size, pos, pad_char);
        }
        if (negative) {
            safe_buf_putc(buf, buf_size, pos, '-');
        }
    }

    for (i = 0U; i < digits_len; i++) {
        safe_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}
#endif

static void fmt_buf_put_i32_basic(char* buf, usize buf_size, usize* pos, i32 value)
{
    bool  negative = false;
    u32   abs_value;
    char  tmp[FMT_U32_TMP_BUF_SIZE];
    usize len;
    usize i;

    if (value < 0) {
        negative  = true;
        abs_value = (u32)(-(value + 1)) + 1U;
    }
    else {
        abs_value = (u32)value;
    }

    if (negative) {
        safe_buf_putc(buf, buf_size, pos, '-');
    }

    len = u32_to_str_safe(tmp, sizeof(tmp), abs_value);
    for (i = 0U; i < len; i++) {
        safe_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}

#if FMT_ENABLE_FLOAT
static void fmt_buf_put_f64(char* buf, usize buf_size, usize* pos, f64 value, u32 precision)
{
    char  tmp[FMT_F64_TO_STR_TMP_BUF_SIZE];
    usize len;
    usize i;

#    if FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    len = f64_to_str_core(tmp, sizeof(tmp), value, precision, true);
#    else
    len = f64_to_str_core(tmp, sizeof(tmp), (f64)(f32)value, precision, false);
#    endif

    for (i = 0U; i < len; i++) {
        safe_buf_putc(buf, buf_size, pos, tmp[i]);
    }
}
#endif

static i32 fmt_vsnprintf_impl(char* buf, usize buf_size, const char* fmt, va_list args,
                              bool fast_return)
{
    usize pos = 0U;

    if (buf == NULL || fmt == NULL || buf_size == 0U) {
        return 0;
    }

    while (*fmt != '\0') {
        if (*fmt != '%') {
            safe_buf_putc(buf, buf_size, &pos, *fmt);
            fmt++;
            continue;
        }

        fmt++;
        if (*fmt == '%') {
            safe_buf_putc(buf, buf_size, &pos, '%');
            fmt++;
            continue;
        }

#if FMT_ENABLE_WIDTH_PRECISION
        bool zero_pad         = false;
        u32  width            = 0U;
        bool has_precision    = false;
        u32  parsed_precision = 0U;

        if (*fmt == '0') {
            zero_pad = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10U + (u32)(*fmt - '0');
            fmt++;
        }

        if (*fmt == '.') {
            has_precision    = true;
            parsed_precision = 0U;
            fmt++;
            while (*fmt >= '0' && *fmt <= '9') {
                parsed_precision = parsed_precision * 10U + (u32)(*fmt - '0');
                fmt++;
            }
        }
#endif

        switch (*fmt) {
        case 'd':
        {
            i32 value = (i32)va_arg(args, int);
#if FMT_ENABLE_WIDTH_PRECISION
            if (width > 0U) {
                fmt_buf_put_i32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                fmt_buf_put_i32_basic(buf, buf_size, &pos, value);
            }
#else
            fmt_buf_put_i32_basic(buf, buf_size, &pos, value);
#endif
            break;
        }
        case 'u':
        {
            u32 value = (u32)va_arg(args, unsigned int);
#if FMT_ENABLE_WIDTH_PRECISION
            if (width > 0U) {
                fmt_buf_put_u32_width(buf, buf_size, &pos, value, width, zero_pad ? '0' : ' ');
            }
            else {
                fmt_buf_put_u32_dec(buf, buf_size, &pos, value);
            }
#else
            fmt_buf_put_u32_dec(buf, buf_size, &pos, value);
#endif
            break;
        }
#if FMT_ENABLE_HEX
        case 'x':
        {
            u32 value = (u32)va_arg(args, unsigned int);
            fmt_buf_put_u32_hex(buf, buf_size, &pos, value, false);
            break;
        }
        case 'X':
        {
            u32 value = (u32)va_arg(args, unsigned int);
            fmt_buf_put_u32_hex(buf, buf_size, &pos, value, true);
            break;
        }
#endif
        case 's':
        {
            const char* value = va_arg(args, const char*);
            fmt_buf_puts(buf, buf_size, &pos, value);
            break;
        }
#if FMT_ENABLE_FLOAT
        case 'f':
        {
            f64 value = (f64)va_arg(args, double);
            u32 precision;
#    if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
            precision = FMT_FIXED_DECIMALS;
#    else
#        if FMT_ENABLE_WIDTH_PRECISION
            precision = has_precision ? parsed_precision : FMT_DEFAULT_PRECISION;
#        else
            precision = FMT_DEFAULT_PRECISION;
#        endif
#    endif
            fmt_buf_put_f64(buf, buf_size, &pos, value, precision);
            break;
        }
#endif
        case '\0': fmt--; break;
        default:
            safe_buf_putc(buf, buf_size, &pos, '%');
            if (*fmt != '\0') {
                safe_buf_putc(buf, buf_size, &pos, *fmt);
            }
            break;
        }

        if (*fmt != '\0') {
            fmt++;
        }
    }

    safe_buf_terminate(buf, buf_size, pos);
    if (pos >= buf_size) {
        if (fast_return) {
            return (i32)(buf_size - 1U);
        }
        return (i32)pos;
    }
    return (i32)pos;
}

i32 fmt_vsnprintf(char* buf, usize buf_size, const char* fmt, va_list args)
{
    return fmt_vsnprintf_impl(buf, buf_size, fmt, args, false);
}

i32 fmt_vsnprintf_fast(char* buf, usize buf_size, const char* fmt, va_list args)
{
    return fmt_vsnprintf_impl(buf, buf_size, fmt, args, true);
}

i32 fmt_snprintf(char* buf, usize buf_size, const char* fmt, ...)
{
    va_list args;
    i32     len;

    if (buf == NULL || fmt == NULL || buf_size == 0U) {
        return 0;
    }

    va_start(args, fmt);
    len = fmt_vsnprintf_impl(buf, buf_size, fmt, args, false);
    va_end(args);

    return len;
}

i32 fmt_snprintf_fast(char* buf, usize buf_size, const char* fmt, ...)
{
    va_list args;
    i32     len;

    if (buf == NULL || fmt == NULL || buf_size == 0U) {
        return 0;
    }

    va_start(args, fmt);
    len = fmt_vsnprintf_impl(buf, buf_size, fmt, args, true);
    va_end(args);

    return len;
}

i32 fmt_sprintf(char* buf, const char* fmt, ...)
{
    va_list args;
    i32     len;

    if (buf == NULL || fmt == NULL) {
        return 0;
    }

    va_start(args, fmt);
    len = fmt_vsnprintf(buf, FMT_SPRINTF_MAX, fmt, args);
    va_end(args);

    return len;
}

#if TEST_ENABLE
#    include "../em_test/test.h"

static i32 test_fmt_vsnprintf_call(char* buf, usize buf_size, const char* fmt, ...)
{
    va_list args;
    i32     len;

    va_start(args, fmt);
    len = fmt_vsnprintf(buf, buf_size, fmt, args);
    va_end(args);

    return len;
}

static i32 test_fmt_vsnprintf_fast_call(char* buf, usize buf_size, const char* fmt, ...)
{
    va_list args;
    i32     len;

    va_start(args, fmt);
    len = fmt_vsnprintf_fast(buf, buf_size, fmt, args);
    va_end(args);

    return len;
}

TEST_CASE(test_u32_to_str_basic)
{
    char  buf[16];
    usize len;

    len      = u32_to_str(buf, 0U);
    buf[len] = '\0';
    TEST_EXPECT_EQ_U32(1U, (u32)len);
    TEST_EXPECT_EQ_STR("0", buf);

    len      = u32_to_str(buf, 4294967295U);
    buf[len] = '\0';
    TEST_EXPECT_EQ_U32(10U, (u32)len);
    TEST_EXPECT_EQ_STR("4294967295", buf);

    TEST_EXPECT_EQ_U32(0U, (u32)u32_to_str(NULL, 7U));
}

TEST_CASE(test_u32_to_str_safe)
{
    char  buf[8];
    char  guard[10] = {'L', 'L', '#', '#', '#', '#', '#', '#', 'R', 'R'};
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

TEST_CASE(test_float_converters)
{
    char buf[64];

#    if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    (void)f32_to_str(buf, 1.2367f, 1U);
    TEST_EXPECT_EQ_STR("1.236", buf);
#    elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    (void)f32_to_str(buf, 1.2367f, 2U);
    TEST_EXPECT_EQ_STR("1.24", buf);
#    else
    (void)f32_to_str(buf, 1.2367f, 2U);
    TEST_EXPECT_EQ_STR("1.23", buf);
#    endif

    (void)f64_to_str_safe(buf, sizeof(buf), -0.125, 3U);
#    if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("-0.125", buf);
#    else
    TEST_EXPECT_EQ_STR("-0.125", buf);
#    endif

    TEST_EXPECT_EQ_U32(0U, (u32)f32_to_str(NULL, 1.0f, 2U));
    TEST_EXPECT_EQ_U32(0U, (u32)f64_to_str_safe(NULL, 8U, 1.0, 2U));
}

TEST_CASE(test_fmt_core_basic)
{
    char buf[96];
    i32  len;

    len = fmt_sprintf(buf, "%d %u %s %%", -12, 42U, "ok");
    TEST_EXPECT_EQ_I32(11, len);
    TEST_EXPECT_EQ_STR("-12 42 ok %", buf);

    len = fmt_sprintf(buf, "%s", NULL);
    TEST_EXPECT_EQ_I32(6, len);
    TEST_EXPECT_EQ_STR("(null)", buf);
}

TEST_CASE(test_fmt_hex_feature)
{
    char buf[32];

    (void)fmt_snprintf(buf, sizeof(buf), "%x %X", 0x2aU, 0x2aU);
#    if FMT_ENABLE_HEX
    TEST_EXPECT_EQ_STR("2a 2A", buf);
#    else
    TEST_EXPECT_EQ_STR("%x %X", buf);
#    endif
}

TEST_CASE(test_fmt_width_precision_feature)
{
    char buf[32];

    (void)fmt_snprintf(buf, sizeof(buf), "%02d", 7);
#    if FMT_ENABLE_WIDTH_PRECISION
    TEST_EXPECT_EQ_STR("07", buf);
#    else
    TEST_EXPECT_EQ_STR("%02d", buf);
#    endif

    (void)fmt_snprintf(buf, sizeof(buf), "%.2q");
#    if FMT_ENABLE_WIDTH_PRECISION
    TEST_EXPECT_EQ_STR("%q", buf);
#    else
    TEST_EXPECT_EQ_STR("%.2q", buf);
#    endif
}

TEST_CASE(test_fmt_float_feature)
{
    char buf[64];

    (void)fmt_snprintf(buf, sizeof(buf), "%f", 1.2367);
#    if FMT_ENABLE_FLOAT
#        if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("1.236", buf);
#        elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    TEST_EXPECT_EQ_STR("1.237", buf);
#        else
    TEST_EXPECT_EQ_STR("1.236", buf);
#        endif
#    else
    TEST_EXPECT_EQ_STR("%f", buf);
#    endif

    (void)fmt_snprintf(buf, sizeof(buf), "%.2f", 1.2367);
#    if FMT_ENABLE_FLOAT
#        if FMT_ENABLE_WIDTH_PRECISION
#            if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("1.236", buf);
#            elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    TEST_EXPECT_EQ_STR("1.24", buf);
#            else
    TEST_EXPECT_EQ_STR("1.23", buf);
#            endif
#        else
#            if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("1.236", buf);
#            elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    TEST_EXPECT_EQ_STR("1.237", buf);
#            else
    TEST_EXPECT_EQ_STR("1.236", buf);
#            endif
#        endif
#    else
#        if FMT_ENABLE_WIDTH_PRECISION
    TEST_EXPECT_EQ_STR("%f", buf);
#        else
    TEST_EXPECT_EQ_STR("%.2f", buf);
#        endif
#    endif
}

TEST_CASE(test_fmt_snprintf_semantics)
{
    char buf[10];
    i32  len;

    len = fmt_snprintf(buf, sizeof(buf), "%s", "123456789");
    TEST_EXPECT_EQ_I32(9, len);
    TEST_EXPECT_EQ_STR("123456789", buf);

    len = fmt_snprintf(buf, 6U, "%s", "123456789");
    TEST_EXPECT_EQ_I32(9, len);
    TEST_EXPECT_EQ_STR("12345", buf);

    len = fmt_snprintf_fast(buf, 6U, "%s", "123456789");
    TEST_EXPECT_EQ_I32(5, len);
    TEST_EXPECT_EQ_STR("12345", buf);
}

TEST_CASE(test_fmt_vsnprintf_fast_semantics)
{
    char buf[8];
    i32  len;

    len = test_fmt_vsnprintf_fast_call(buf, sizeof(buf), "%s", "123456789");
    TEST_EXPECT_EQ_I32(7, len);
    TEST_EXPECT_EQ_STR("1234567", buf);

    len = test_fmt_vsnprintf_fast_call(buf, 1U, "A");
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
    TEST_EXPECT_EQ_I32(0, test_fmt_vsnprintf_call(buf, sizeof(buf), NULL));

    TEST_EXPECT_EQ_I32(0, test_fmt_vsnprintf_fast_call(NULL, sizeof(buf), "x"));
    TEST_EXPECT_EQ_I32(0, test_fmt_vsnprintf_fast_call(buf, 0U, "x"));
    TEST_EXPECT_EQ_I32(0, test_fmt_vsnprintf_fast_call(buf, sizeof(buf), NULL));
}

#endif
