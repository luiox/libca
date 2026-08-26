/// @file vhil.h
/// @author Canrad
/// @brief 虚拟硬件接口层，Virtual Hardware Interface Layer
/// 提供简答的硬件接口的定义
/// @version 0.1
/// @date 2025-08-14
///
/// @copyright Copyright (c) 2025
///
#ifndef VHIL_H
#define VHIL_H

#include <em_base/datatype.h>

typedef enum
{
    VHAL_OK      = 0x00U,
    VHAL_ERROR   = 0x01U,
    VHAL_BUSY    = 0x02U,
    VHAL_TIMEOUT = 0x03U
} vhil_state_type_t;

// gpio
typedef vhil_state_type_t (*gpio_write_pin_fn_t)(void* gpio, u16 pin, u8 value);
typedef u8 (*gpio_read_pin_fn_t)(void* gpio, u16 pin);

// i2c

#define I2C_MEM_ADDR_SIZE_8BIT            0x01
#define I2C_MEM_ADDR_SIZE_16BIT           0x010

typedef vhil_state_type_t (*i2c_master_write_func_t)(void* i2c_extra_data, u16 dev_addr, u8* data,
                                                     u16 data_size, u32 timeout);
typedef vhil_state_type_t (*i2c_master_read_func_t)(void* i2c_extra_data, u16 dev_addr, u8* data,
                                                    u16 data_size, u32 timeout);
typedef vhil_state_type_t (*i2c_slave_write_func_t)(void* i2c_extra_data, u16 dev_addr, u8* data,
                                                    u16 data_size, u32 timeout);
typedef vhil_state_type_t (*i2c_slave_read_func_t)(void* i2c_extra_data, u16 dev_addr, u8* data,
                                                   u16 data_size, u32 timeout);
typedef vhil_state_type_t (*i2c_mem_write_func_t)(void* i2c_extra_data, u16 dev_addr, u16 mem_addr,
                                                  u16 mem_addr_size, u8* data, u16 data_size,
                                                  u32 timeout);
typedef vhil_state_type_t (*i2c_mem_read_func_t)(void* i2c_extra_data, u16 dev_addr, u16 mem_addr,
                                                 u16 mem_addr_size, u8* data, u16 data_size,
                                                 u32 timeout);
typedef vhil_state_type_t (*i2c_is_device_ready_func_t)(void* i2c_extra_dat, u16 dev_addr, u32 trials,
                                                 u32 timeout);


#endif   // !VHIL_H
