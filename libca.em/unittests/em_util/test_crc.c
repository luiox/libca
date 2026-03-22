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
