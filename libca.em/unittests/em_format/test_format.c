/* Auto-migrated from src/em_format/format.c test blocks */
#include "format.h"

#include <em_test/test.h>

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

#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    (void)f32_to_str(buf, 1.2367f, 1U);
    TEST_EXPECT_EQ_STR("1.236", buf);
#elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    (void)f32_to_str(buf, 1.2367f, 2U);
    TEST_EXPECT_EQ_STR("1.24", buf);
#else
    (void)f32_to_str(buf, 1.2367f, 2U);
    TEST_EXPECT_EQ_STR("1.23", buf);
#endif

    (void)f64_to_str_safe(buf, sizeof(buf), -0.125, 3U);
#if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("-0.125", buf);
#else
    TEST_EXPECT_EQ_STR("-0.125", buf);
#endif

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
#if FMT_ENABLE_HEX
    TEST_EXPECT_EQ_STR("2a 2A", buf);
#else
    TEST_EXPECT_EQ_STR("%x %X", buf);
#endif
}

TEST_CASE(test_fmt_width_precision_feature)
{
    char buf[32];

    (void)fmt_snprintf(buf, sizeof(buf), "%02d", 7);
#if FMT_ENABLE_WIDTH_PRECISION
    TEST_EXPECT_EQ_STR("07", buf);
#else
    TEST_EXPECT_EQ_STR("%02d", buf);
#endif

    (void)fmt_snprintf(buf, sizeof(buf), "%.2q");
#if FMT_ENABLE_WIDTH_PRECISION
    TEST_EXPECT_EQ_STR("%q", buf);
#else
    TEST_EXPECT_EQ_STR("%.2q", buf);
#endif
}

TEST_CASE(test_fmt_float_feature)
{
    char buf[64];

    (void)fmt_snprintf(buf, sizeof(buf), "%f", 1.2367);
#if FMT_ENABLE_FLOAT
#    if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("1.236", buf);
#    elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    TEST_EXPECT_EQ_STR("1.237", buf);
#    else
    TEST_EXPECT_EQ_STR("1.236", buf);
#    endif
#else
    TEST_EXPECT_EQ_STR("%f", buf);
#endif

    (void)fmt_snprintf(buf, sizeof(buf), "%.2f", 1.2367);
#if FMT_ENABLE_FLOAT
#    if FMT_ENABLE_WIDTH_PRECISION
#        if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("1.236", buf);
#        elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    TEST_EXPECT_EQ_STR("1.24", buf);
#        else
    TEST_EXPECT_EQ_STR("1.23", buf);
#        endif
#    else
#        if FMT_FLOAT_MODE == FMT_FLOAT_MODE_FIXED
    TEST_EXPECT_EQ_STR("1.236", buf);
#        elif FMT_FLOAT_MODE == FMT_FLOAT_MODE_NORMAL
    TEST_EXPECT_EQ_STR("1.237", buf);
#        else
    TEST_EXPECT_EQ_STR("1.236", buf);
#        endif
#    endif
#else
#    if FMT_ENABLE_WIDTH_PRECISION
    TEST_EXPECT_EQ_STR("%f", buf);
#    else
    TEST_EXPECT_EQ_STR("%.2f", buf);
#    endif
#endif
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
