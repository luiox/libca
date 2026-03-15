#include "as5600.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK void port_as5600_i2c_write(void* hi2c, u8 dev_addr, u8 reg_addr, const u8* data, u16 len)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(reg_addr);
    unused_param(data);
    unused_param(len);
}

CA_WEAK void port_as5600_i2c_read(void* hi2c, u8 dev_addr, u8 reg_addr, u8* data, u16 len)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(reg_addr);
    unused_param(data);
    unused_param(len);
}
