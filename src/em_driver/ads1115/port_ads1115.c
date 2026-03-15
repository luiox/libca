#include "ads1115.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK i32 port_ads1115_i2c_write(void* hi2c, u8 dev_addr, u8 reg_addr, const u8* data, u16 size)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(reg_addr);
    unused_param(data);
    unused_param(size);
    return 0;
}

CA_WEAK i32 port_ads1115_i2c_read(void* hi2c, u8 dev_addr, u8 reg_addr, u8* data, u16 size)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(reg_addr);
    unused_param(data);
    unused_param(size);
    return 0;
}

CA_WEAK void port_ads1115_delay_ms(u32 ms)
{
    unused_param(ms);
}
