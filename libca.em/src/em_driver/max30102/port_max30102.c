#include "max30102.h"
#include <em_base/compiler_compat.h>

CA_WEAK i32 port_max30102_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size,
                                    u8* data, u16 data_size, u32 timeout)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(mem_addr);
    unused_param(mem_addr_size);
    unused_param(data);
    unused_param(data_size);
    unused_param(timeout);
    return 0;
}

CA_WEAK i32 port_max30102_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size,
                                   u8* data, u16 data_size, u32 timeout)
{
    unused_param(hi2c);
    unused_param(dev_addr);
    unused_param(mem_addr);
    unused_param(mem_addr_size);
    unused_param(data);
    unused_param(data_size);
    unused_param(timeout);
    return 0;
}
