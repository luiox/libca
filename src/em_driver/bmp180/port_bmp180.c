#include "bmp180.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK i32 port_bmp180_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout)
{
    (void)hi2c;
    (void)dev_addr;
    (void)mem_addr;
    (void)mem_addr_size;
    (void)data;
    (void)data_size;
    (void)timeout;
    return 0;
}

CA_WEAK i32 port_bmp180_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout)
{
    (void)hi2c;
    (void)dev_addr;
    (void)mem_addr;
    (void)mem_addr_size;
    (void)data;
    (void)data_size;
    (void)timeout;
    return 0;
}

CA_WEAK void port_bmp180_delay_ms(u32 ms)
{
    (void)ms;
}
