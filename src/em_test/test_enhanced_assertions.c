/**
 * @file test_enhanced_assertions.c
 * @brief 精确类型断言宏使用示例和测试
 * 
 * 本文件演示了新的精确类型断言宏的用法，包括：
 * - TEST_ASSERT_EQUAL_U8/U16/U32: 无符号精确类型断言
 * - TEST_ASSERT_EQUAL_I8/I16/I32: 有符号精确类型断言
 * - TEST_ASSERT_EQUAL_U8/U16/U32_BITS: 位宽比较宏（处理符号混用）
 */

#include <stdint.h>
#include "test.h"

// 启用测试
#define TEST_ENABLE 1
#define TEST_SELF_MAIN 0

/**
 * @brief 测试精确类型断言 - 无符号类型
 */
TEST_CASE(test_exact_type_unsigned)
{
    uint8_t  u8_val  = 0xAB;
    uint16_t u16_val = 0xCD12;
    uint32_t u32_val = 0xEF345678;

    // 精确类型断言 - 无符号8位
    TEST_ASSERT_EQUAL_U8(0xAB, u8_val);
    TEST_ASSERT_EQUAL_U8(0, 0);
    TEST_ASSERT_EQUAL_U8(255, 0xFF);

    // 精确类型断言 - 无符号16位
    TEST_ASSERT_EQUAL_U16(0xCD12, u16_val);
    TEST_ASSERT_EQUAL_U16(0, 0);
    TEST_ASSERT_EQUAL_U16(65535, 0xFFFF);

    // 精确类型断言 - 无符号32位
    TEST_ASSERT_EQUAL_U32(0xEF345678, u32_val);
    TEST_ASSERT_EQUAL_U32(0, 0);
    TEST_ASSERT_EQUAL_U32(4294967295U, 0xFFFFFFFFU);
}

/**
 * @brief 测试精确类型断言 - 有符号类型
 */
TEST_CASE(test_exact_type_signed)
{
    int8_t  i8_val  = -42;
    int16_t i16_val = -12345;
    int32_t i32_val = -987654321;

    // 精确类型断言 - 有符号8位
    TEST_ASSERT_EQUAL_I8(-42, i8_val);
    TEST_ASSERT_EQUAL_I8(0, 0);
    TEST_ASSERT_EQUAL_I8(-128, (int8_t)-128);
    TEST_ASSERT_EQUAL_I8(127, 127);

    // 精确类型断言 - 有符号16位
    TEST_ASSERT_EQUAL_I16(-12345, i16_val);
    TEST_ASSERT_EQUAL_I16(0, 0);
    TEST_ASSERT_EQUAL_I16(-32768, (int16_t)-32768);
    TEST_ASSERT_EQUAL_I16(32767, 32767);

    // 精确类型断言 - 有符号32位
    TEST_ASSERT_EQUAL_I32(-987654321, i32_val);
    TEST_ASSERT_EQUAL_I32(0, 0);
    TEST_ASSERT_EQUAL_I32(-2147483648LL, (int32_t)-2147483648LL);
    TEST_ASSERT_EQUAL_I32(2147483647, 2147483647);
}

/**
 * @brief 测试位宽比较宏 - 处理符号混用场景
 * 
 * 这是最重要的功能：处理有符号和无符号相同位宽类型的混用问题
 * 例如：int8_t 的 -1 和 uint8_t 的 255 在二进制上是相同的（0xFF）
 */
TEST_CASE(test_bitwise_comparison)
{
    // 8位场景：-1 (int8_t) == 255 (uint8_t) 在二进制上都是 0xFF
    int8_t  i8_neg1 = -1;
    uint8_t u8_255  = 255;
    
    // 使用 BITS 宏进行比较，忽略符号，只比较二进制内容
    TEST_ASSERT_EQUAL_U8_BITS(i8_neg1, u8_255);  // 两者都是 0xFF
    
    // 其他测试用例
    int8_t  i8_min  = -128;  // 0x80
    uint8_t u8_128  = 128;   // 0x80
    TEST_ASSERT_EQUAL_U8_BITS(i8_min, u8_128);
    
    // 16位场景
    int16_t i16_val = -1;     // 0xFFFF
    uint16_t u16_val = 65535; // 0xFFFF
    TEST_ASSERT_EQUAL_U16_BITS(i16_val, u16_val);
    
    int16_t i16_min = -32768;  // 0x8000
    uint16_t u16_32768 = 32768; // 0x8000
    TEST_ASSERT_EQUAL_U16_BITS(i16_min, u16_32768);
    
    // 32位场景
    int32_t i32_val = -1;          // 0xFFFFFFFF
    uint32_t u32_val = 4294967295U; // 0xFFFFFFFF
    TEST_ASSERT_EQUAL_U32_BITS(i32_val, u32_val);
}

/**
 * @brief 测试为什么需要精确类型断言（对比演示）
 * 
 * 这个测试用例演示了传统 INT/UINT 断言的问题
 */
TEST_CASE(test_why_exact_types_matter)
{
    int8_t  signed_val   = -1;   // 二进制: 0xFF
    uint8_t unsigned_val = 255;  // 二进制: 0xFF
    
    // 问题：使用传统 INT 断言时，整型提升会导致意外的比较结果
    // TEST_ASSERT_EQUAL_INT(signed_val, unsigned_val);  // 失败！比较的是 -1 != 255
    
    // 解决方案1：使用 BITS 宏（推荐用于二进制数据比较）
    TEST_ASSERT_EQUAL_U8_BITS(signed_val, unsigned_val);  // 成功！两者都是 0xFF
    
    // 解决方案2：明确指定类型
    TEST_ASSERT_EQUAL_I8(-1, signed_val);      // 成功！按有符号比较
    TEST_ASSERT_EQUAL_U8(255, unsigned_val);   // 成功！按无符号比较
}

/**
 * @brief 测试网络/通信协议数据（实际应用场景）
 * 
 * 在嵌入式通信协议中，经常需要比较原始字节数据
 */
TEST_CASE(test_protocol_byte_comparison)
{
    // 模拟接收到的数据包
    uint8_t packet_header[] = {0xAA, 0x55, 0x01, 0xFF};
    
    // 验证同步字节
    TEST_ASSERT_EQUAL_U8(0xAA, packet_header[0]);
    TEST_ASSERT_EQUAL_U8(0x55, packet_header[1]);
    
    // 验证命令字节
    TEST_ASSERT_EQUAL_U8(0x01, packet_header[2]);
    
    // 验证校验和（可能是从有符号计算得到，但存储为字节）
    int8_t checksum_calc = -1;  // 计算得到的校验和（有符号）
    TEST_ASSERT_EQUAL_U8_BITS(checksum_calc, packet_header[3]);  // 比较二进制值
}
