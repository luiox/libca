/* Auto-migrated from src/em_util/crc.c test blocks */
#include "crc.h"

#include <em_test/test.h>
#include <string.h>

// CRC 模块单元测试
TEST_CASE(crc_standard_vectors)
{
    const char* data = "123456789";
    usize       len  = strlen(data);

    // CRC32 IEEE
    u32 c32_fast = crc32_ieee_fast(data, len);
    u32 c32_slow = crc32_ieee(data, len);
    TEST_ASSERT(c32_fast == 0xCBF43926);
    TEST_ASSERT(c32_slow == 0xCBF43926);

    // CRC16 Modbus
    u16 c16_modbus_fast = crc16_modbus_fast(data, len);
    u16 c16_modbus_slow = crc16_modbus(data, len);
    TEST_ASSERT(c16_modbus_fast == 0x4B37);
    TEST_ASSERT(c16_modbus_slow == 0x4B37);

    // CRC16 XMODEM
    u16 c16_xmodem_fast = crc16_xmodem_fast(data, len);
    u16 c16_xmodem_slow = crc16_xmodem(data, len);
    TEST_ASSERT(c16_xmodem_fast == 0x31C3);
    TEST_ASSERT(c16_xmodem_slow == 0x31C3);

    // CRC16 YMODEM
    u16 c16_ymodem_fast = crc16_ymodem_fast(data, len);
    u16 c16_ymodem_slow = crc16_ymodem(data, len);
    TEST_ASSERT(c16_ymodem_fast == 0x31C3);
    TEST_ASSERT(c16_ymodem_slow == 0x31C3);
}

TEST_CASE(crc32_ieee_ex_rolling)
{
    const char* data = "123456789";
    usize       len  = strlen(data);

    // 整段一次计算的基准值
    u32 whole = crc32_ieee_fast(data, len);
    TEST_ASSERT(whole == 0xCBF43926);

    // 首块传 0，结果必须与整段一致
    TEST_ASSERT(crc32_ieee_ex(data, len, 0) == whole);

    // 分三段滚动计算，最终结果与整段一致
    u32 r = crc32_ieee_ex(data, 3, 0);
    r     = crc32_ieee_ex(data + 3, 3, r);
    r     = crc32_ieee_ex(data + 6, len - 6, r);
    TEST_ASSERT(r == whole);

    // 空数据滚动：返回值不变
    TEST_ASSERT(crc32_ieee_ex(data, 0, whole) == whole);
}
