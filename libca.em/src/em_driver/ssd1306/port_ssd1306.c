#include "port_ssd1306.h"
#include "ssd1306.h"
// #include "mcu_wrapper.h"

// static void ssd1306_port_delay_ms(u32 ms)
// {
//    HAL_Delay(ms);
// }

// static void ssd1306_port_i2c_mem_write(void* i2c_extra_data, u16 dev_addr, u8 mem_addr, u16
// mem_addr_size,
//                                        u8* data, u16 data_size, u32 timeout)
// {
//     HAL_I2C_Mem_Write((I2C_HandleTypeDef*)i2c_extra_data, dev_addr, mem_addr, mem_addr_size,
//     data, data_size,
//                   timeout);
// }

// static const ssd1306_port_t g_ssd1306_port = {
//     .delay_ms = ssd1306_port_delay_ms,
//     .i2c_mem_write = ssd1306_port_i2c_mem_write,

// };

// bool ssd1306_port_init(void)
// {
//     ssd1306_bind_port(&g_ssd1306_port);
//     return ssd1306_port_is_registered();
// }
