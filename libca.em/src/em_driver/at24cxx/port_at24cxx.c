#include "at24cxx.h"
#include <em_base/compiler_compat.h>

/// @brief 外部隐式注入的弱符号接口实现
///  

CA_WEAK void port_at24cxx_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(mem_addr);
    unused_param(mem_addr_size);
    unused_param(data);
    unused_param(data_size);
    unused_param(timeout);
}

CA_WEAK void port_at24cxx_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(mem_addr);
    unused_param(mem_addr_size);
    unused_param(data);
    unused_param(data_size);
    unused_param(timeout);
}