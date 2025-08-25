#include <stdio.h>
#include "base/datatype.h"
#include "test_helper.h"

// 测试大端读取 u16
TEST_CASE_BEGIN(test_big_endian_read_u16)
{
    u8 bytes[2] = {0x12, 0x34};
    u16 result = big_endian_read_u16(bytes);
    ASSERT_EQ(result, 0x1234);
}
TEST_CASE_END(test_big_endian_read_u16)

// 测试大端读取 s16
TEST_CASE_BEGIN(test_big_endian_read_s16)
{
    u8 bytes[2] = {0xFF, 0xFE}; // -2 in big-endian
    s16 result = big_endian_read_s16(bytes);
    ASSERT_EQ(result, -2);
}
TEST_CASE_END(test_big_endian_read_s16)

// 测试大端读取 u32
TEST_CASE_BEGIN(test_big_endian_read_u32)
{
    u8 bytes[4] = {0x12, 0x34, 0x56, 0x78};
    u32 result = big_endian_read_u32(bytes);
    ASSERT_EQ(result, 0x12345678);
}
TEST_CASE_END(test_big_endian_read_u32)

// 测试大端读取 s32
TEST_CASE_BEGIN(test_big_endian_read_s32)
{
    u8 bytes[4] = {0xFF, 0xFF, 0xFF, 0xFE}; // -2 in big-endian
    s32 result = big_endian_read_s32(bytes);
    ASSERT_EQ(result, -2);
}
TEST_CASE_END(test_big_endian_read_s32)

// 测试小端读取 u16
TEST_CASE_BEGIN(test_little_endian_read_u16)
{
    u8 bytes[2] = {0x34, 0x12};
    u16 result = little_endian_read_u16(bytes);
    ASSERT_EQ(result, 0x1234);
}
TEST_CASE_END(test_little_endian_read_u16)

// 测试小端读取 s16
TEST_CASE_BEGIN(test_little_endian_read_s16)
{
    u8 bytes[2] = {0xFE, 0xFF}; // -2 in little-endian
    s16 result = little_endian_read_s16(bytes);
    ASSERT_EQ(result, (s16)0xFFFE);
}
TEST_CASE_END(test_little_endian_read_s16)

// 测试小端读取 u32
TEST_CASE_BEGIN(test_little_endian_read_u32)
{
    u8 bytes[4] = {0x78, 0x56, 0x34, 0x12};
    u32 result = little_endian_read_u32(bytes);
    ASSERT_EQ(result, 0x12345678);
}
TEST_CASE_END(test_little_endian_read_u32)

// 测试小端读取 s32
TEST_CASE_BEGIN(test_little_endian_read_s32)
{
    u8 bytes[4] = {0xFE, 0xFF, 0xFF, 0xFF}; // -2 in little-endian
    s32 result = little_endian_read_s32(bytes);
    ASSERT_EQ(result, -2);
}
TEST_CASE_END(test_little_endian_read_s32)

// 测试大端写入 u16
TEST_CASE_BEGIN(test_big_endian_write_u16)
{
    u8 bytes[2];
    big_endian_write_u16(bytes, 0x1234);
    ASSERT_EQ(bytes[0], 0x12);
    ASSERT_EQ(bytes[1], 0x34);
}
TEST_CASE_END(test_big_endian_write_u16)

// 测试大端写入 s16
TEST_CASE_BEGIN(test_big_endian_write_s16)
{
    u8 bytes[2];
    big_endian_write_s16(bytes, -2);
    ASSERT_EQ(bytes[0], 0xFF);
    ASSERT_EQ(bytes[1], 0xFE);
}
TEST_CASE_END(test_big_endian_write_s16)

// 测试大端写入 u32
TEST_CASE_BEGIN(test_big_endian_write_u32)
{
    u8 bytes[4];
    big_endian_write_u32(bytes, 0x12345678);
    ASSERT_EQ(bytes[0], 0x12);
    ASSERT_EQ(bytes[1], 0x34);
    ASSERT_EQ(bytes[2], 0x56);
    ASSERT_EQ(bytes[3], 0x78);
}
TEST_CASE_END(test_big_endian_write_u32)

// 测试大端写入 s32
TEST_CASE_BEGIN(test_big_endian_write_s32)
{
    u8 bytes[4];
    big_endian_write_s32(bytes, -2);
    ASSERT_EQ(bytes[0], 0xFF);
    ASSERT_EQ(bytes[1], 0xFF);
    ASSERT_EQ(bytes[2], 0xFF);
    ASSERT_EQ(bytes[3], 0xFE);
}
TEST_CASE_END(test_big_endian_write_s32)

// 测试小端写入 u16
TEST_CASE_BEGIN(test_little_endian_write_u16)
{
    u8 bytes[2];
    little_endian_write_u16(bytes, 0x1234);
    ASSERT_EQ(bytes[0], 0x34);
    ASSERT_EQ(bytes[1], 0x12);
}
TEST_CASE_END(test_little_endian_write_u16)

// 测试小端写入 s16
TEST_CASE_BEGIN(test_little_endian_write_s16)
{
    u8 bytes[2];
    little_endian_write_s16(bytes, -2);
    ASSERT_EQ(bytes[0], 0xFE);
    ASSERT_EQ(bytes[1], 0xFF);
}
TEST_CASE_END(test_little_endian_write_s16)

// 测试小端写入 u32
TEST_CASE_BEGIN(test_little_endian_write_u32)
{
    u8 bytes[4];
    little_endian_write_u32(bytes, 0x12345678);
    ASSERT_EQ(bytes[0], 0x78);
    ASSERT_EQ(bytes[1], 0x56);
    ASSERT_EQ(bytes[2], 0x34);
    ASSERT_EQ(bytes[3], 0x12);
}
TEST_CASE_END(test_little_endian_write_u32)

// 测试小端写入 s32
TEST_CASE_BEGIN(test_little_endian_write_s32)
{
    u8 bytes[4];
    little_endian_write_s32(bytes, -2);
    ASSERT_EQ(bytes[0], 0xFE);
    ASSERT_EQ(bytes[1], 0xFF);
    ASSERT_EQ(bytes[2], 0xFF);
    ASSERT_EQ(bytes[3], 0xFF);
}
TEST_CASE_END(test_little_endian_write_s32)

// 主测试函数
int main()
{
    printf("Running all endian conversion tests...\n");

    test_big_endian_read_u16();
    test_big_endian_read_s16();
    test_big_endian_read_u32();
    test_big_endian_read_s32();

    test_little_endian_read_u16();
    test_little_endian_read_s16();
    test_little_endian_read_u32();
    test_little_endian_read_s32();

    test_big_endian_write_u16();
    test_big_endian_write_s16();
    test_big_endian_write_u32();
    test_big_endian_write_s32();

    test_little_endian_write_u16();
    test_little_endian_write_s16();
    test_little_endian_write_u32();
    test_little_endian_write_s32();

    // printf("\nAll tests completed!\n");
    return 0;
}
