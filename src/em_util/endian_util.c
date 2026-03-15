#include "endian_util.h"

static i8 endian_flag = -1;
// 是否是小端序
bool is_little_endian(void)
{
    if (endian_flag != -1) {
        return endian_flag == 1;
    }
    union
    {
        u8  bytes[2];
        u16 value;
    } u;
    u.value = 0x0102;
    if (u.bytes[0] == 0x01) {
        endian_flag = 1;
        return true;
    }
    endian_flag = 0;
    return false;
}

// 是否是大端序
bool is_big_endian(void)
{
    return !is_little_endian();
}

#if TEST_ENABLE

#include <em_test/test.h>

// 测试大端读取 u16
TEST_CASE(test_big_endian_read_u16)
{
    u8  bytes[2] = {0x12, 0x34};
    u16 result   = big_endian_read_u16(bytes);
    TEST_ASSERT_EQUAL_INT(result, 0x1234);
}

// 测试大端读取 s16
TEST_CASE(test_big_endian_read_s16)
{
    u8  bytes[2] = {0xFF, 0xFE};   // -2 in big-endian
    i16 result   = big_endian_read_s16(bytes);
    TEST_ASSERT_EQUAL_INT(result, -2);
}

// 测试大端读取 u32
TEST_CASE(test_big_endian_read_u32)
{
    u8  bytes[4] = {0x12, 0x34, 0x56, 0x78};
    u32 result   = big_endian_read_u32(bytes);
    TEST_ASSERT_EQUAL_INT(result, 0x12345678);
}

// 测试大端读取 s32
TEST_CASE(test_big_endian_read_s32)
{
    u8  bytes[4] = {0xFF, 0xFF, 0xFF, 0xFE};   // -2 in big-endian
    i32 result   = big_endian_read_s32(bytes);
    TEST_ASSERT_EQUAL_INT(result, -2);
}

// 测试小端读取 u16
TEST_CASE(test_little_endian_read_u16)
{
    u8  bytes[2] = {0x34, 0x12};
    u16 result   = little_endian_read_u16(bytes);
    TEST_ASSERT_EQUAL_INT(result, 0x1234);
}

// 测试小端读取 s16
TEST_CASE(test_little_endian_read_s16)
{
    u8  bytes[2] = {0xFE, 0xFF};   // -2 in little-endian
    i16 result   = little_endian_read_s16(bytes);
    TEST_ASSERT_EQUAL_INT(result, (i16)0xFFFE);
}

// 测试小端读取 u32
TEST_CASE(test_little_endian_read_u32)
{
    u8  bytes[4] = {0x78, 0x56, 0x34, 0x12};
    u32 result   = little_endian_read_u32(bytes);
    TEST_ASSERT_EQUAL_INT(result, 0x12345678);
}

// 测试小端读取 s32
TEST_CASE(test_little_endian_read_s32)
{
    u8  bytes[4] = {0xFE, 0xFF, 0xFF, 0xFF};   // -2 in little-endian
    i32 result   = little_endian_read_s32(bytes);
    TEST_ASSERT_EQUAL_INT(result, -2);
}

// 测试大端写入 u16
TEST_CASE(test_big_endian_write_u16)
{
    u8 bytes[2];
    big_endian_write_u16(bytes, 0x1234);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0x12);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0x34);
}

// 测试大端写入 s16
TEST_CASE(test_big_endian_write_s16)
{
    u8 bytes[2];
    big_endian_write_s16(bytes, -2);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0xFF);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0xFE);
}

// 测试大端写入 u32
TEST_CASE(test_big_endian_write_u32)
{
    u8 bytes[4];
    big_endian_write_u32(bytes, 0x12345678);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0x12);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0x34);
    TEST_ASSERT_EQUAL_INT(bytes[2], 0x56);
    TEST_ASSERT_EQUAL_INT(bytes[3], 0x78);
}

// 测试大端写入 s32
TEST_CASE(test_big_endian_write_s32)
{
    u8 bytes[4];
    big_endian_write_s32(bytes, -2);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0xFF);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0xFF);
    TEST_ASSERT_EQUAL_INT(bytes[2], 0xFF);
    TEST_ASSERT_EQUAL_INT(bytes[3], 0xFE);
}

// 测试小端写入 u16
TEST_CASE(test_little_endian_write_u16)
{
    u8 bytes[2];
    little_endian_write_u16(bytes, 0x1234);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0x34);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0x12);
}

// 测试小端写入 s16
TEST_CASE(test_little_endian_write_s16)
{
    u8 bytes[2];
    little_endian_write_s16(bytes, -2);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0xFE);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0xFF);
}

// 测试小端写入 u32
TEST_CASE(test_little_endian_write_u32)
{
    u8 bytes[4];
    little_endian_write_u32(bytes, 0x12345678);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0x78);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0x56);
    TEST_ASSERT_EQUAL_INT(bytes[2], 0x34);
    TEST_ASSERT_EQUAL_INT(bytes[3], 0x12);
}

// 测试小端写入 s32
TEST_CASE(test_little_endian_write_s32)
{
    u8 bytes[4];
    little_endian_write_s32(bytes, -2);
    TEST_ASSERT_EQUAL_INT(bytes[0], 0xFE);
    TEST_ASSERT_EQUAL_INT(bytes[1], 0xFF);
    TEST_ASSERT_EQUAL_INT(bytes[2], 0xFF);
    TEST_ASSERT_EQUAL_INT(bytes[3], 0xFF);
}

#endif
