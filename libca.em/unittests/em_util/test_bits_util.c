#include "bits_util.h"

#include <em_test/test.h>

static u8 reverse_u8(u8 value)
{
    u8 out = 0u;
    for (u8 i = 0; i < 8u; i++) {
        out = (u8)((out << 1) | (value & 0x01u));
        value = (u8)(value >> 1);
    }
    return out;
}

TEST_CASE(bits_util_write_read_be_byte)
{
    u8  data[1] = {0};
    u32 value = 0;

    TEST_ASSERT_EQUAL_INT(0, bits_write_be(data, 1, 0, 8, 0x96u));
    TEST_ASSERT_EQUAL_INT(0x96, data[0]);

    TEST_ASSERT_EQUAL_INT(0, bits_read_be(data, 1, 0, 8, &value));
    TEST_ASSERT_EQUAL_INT(0x96, value);
}

TEST_CASE(bits_util_write_read_le_byte)
{
    u8  data[1] = {0};
    u8  expected = reverse_u8(0x96u);
    u32 value = 0;

    TEST_ASSERT_EQUAL_INT(0, bits_write_le(data, 1, 0, 8, 0x96u));
    TEST_ASSERT_EQUAL_INT(expected, data[0]);

    TEST_ASSERT_EQUAL_INT(0, bits_read_le(data, 1, 0, 8, &value));
    TEST_ASSERT_EQUAL_INT(0x96, value);
}

TEST_CASE(bits_util_write_read_be_cross_byte)
{
    u8  data[2] = {0};
    u32 value = 0;

    TEST_ASSERT_EQUAL_INT(0, bits_write_be(data, 2, 4, 8, 0x35u));
    TEST_ASSERT_EQUAL_INT(0x03, data[0]);
    TEST_ASSERT_EQUAL_INT(0x50, data[1]);

    TEST_ASSERT_EQUAL_INT(0, bits_read_be(data, 2, 4, 8, &value));
    TEST_ASSERT_EQUAL_INT(0x35, value);
}

TEST_CASE(bits_util_write_read_le_cross_byte)
{
    u8  data[2] = {0};
    u32 value = 0;

    TEST_ASSERT_EQUAL_INT(0, bits_write_le(data, 2, 4, 8, 0x35u));
    TEST_ASSERT_EQUAL_INT(0x0A, data[0]);
    TEST_ASSERT_EQUAL_INT(0xC0, data[1]);

    TEST_ASSERT_EQUAL_INT(0, bits_read_le(data, 2, 4, 8, &value));
    TEST_ASSERT_EQUAL_INT(0x35, value);
}

TEST_CASE(bits_util_read_known_stream)
{
    u8  data[2] = {0xB3u, 0x5Cu};
    u32 value = 0;

    TEST_ASSERT_EQUAL_INT(0, bits_read_be(data, 2, 4, 8, &value));
    TEST_ASSERT_EQUAL_INT(0x35, value);

    TEST_ASSERT_EQUAL_INT(0, bits_read_le(data, 2, 4, 8, &value));
    TEST_ASSERT_EQUAL_INT(0xAC, value);
}

TEST_CASE(bits_util_param_errors)
{
    u8  data[1] = {0};
    u32 value = 0;

    TEST_ASSERT_EQUAL_INT(BITS_UTIL_EINVAL, bits_read_le(NULL, 1, 0, 1, &value));
    TEST_ASSERT_EQUAL_INT(BITS_UTIL_EINVAL, bits_read_le(data, 1, 0, 0, &value));
    TEST_ASSERT_EQUAL_INT(BITS_UTIL_ERANGE, bits_read_le(data, 1, 8, 1, &value));
    TEST_ASSERT_EQUAL_INT(BITS_UTIL_ERANGE, bits_write_be(data, 1, 7, 2, 1));
}

TEST_CASE(bits_util_bit_macros)
{
    u8 bits = 0u;

    bits_set(bits, 3, 1);
    TEST_ASSERT_EQUAL_INT(0x08, bits);

    bits_toggle(bits, 3);
    TEST_ASSERT_EQUAL_INT(0x00, bits);

    bits_set(bits, 0, 1);
    TEST_ASSERT_EQUAL_INT(1, bits_get(bits, 0));
    TEST_ASSERT(bits_check_bit(bits, 0));
}

#ifdef HAS_INT64
TEST_CASE(bits_util_roundtrip_64_be)
{
    u8  data[8] = {0};
    u64 value = 0x0123456789ABCDEFULL;
    u64 out = 0ULL;

    TEST_ASSERT_EQUAL_INT(0, bits_write64_be(data, 8, 0, 64, value));
    TEST_ASSERT_EQUAL_INT(0, bits_read64_be(data, 8, 0, 64, &out));
    TEST_ASSERT(out == value);
}

TEST_CASE(bits_util_roundtrip_64_le)
{
    u8  data[8] = {0};
    u64 value = 0x0123456789ABCDEFULL;
    u64 out = 0ULL;

    TEST_ASSERT_EQUAL_INT(0, bits_write64_le(data, 8, 0, 64, value));
    TEST_ASSERT_EQUAL_INT(0, bits_read64_le(data, 8, 0, 64, &out));
    TEST_ASSERT(out == value);
}
#endif
