/******************************************************************************
 * @file    driver_soft_i2c.h
 * @author  canrad
 * @version v1.2
 * @date    2024-05-12
 * update
 * v1.2:    2025-08-1
 * @brief   软件iic驱动
 ******************************************************************************/

#ifndef MTLIB_DRIVER_SOFT_I2C_H
#define MTLIB_DRIVER_SOFT_I2C_H

#include "../em_base/datatype.h"
#include "vhil.h"

// 写控制bit
#define I2C_WRITE 0
// 读控制bit
#define I2C_READ 1

typedef struct
{
    // scl的额外信息
    void* scl;
    // sda的额外信息
    void* sda;
    // 设置gpio模式
    void (*gpio_set_output_mode)(void* gpio_extra_data);
    void (*gpio_set_input_mode)(void* gpio_extra_data);
    // 读取高电平为1，读取低电平为0
    u8 (*gpio_read)(void* gpio_extra_data);
    void (*gpio_write)(void* gpio_extra_data, u8 value);
    void (*delay_us)(u32 us);
} soft_i2c_t;

void soft_i2c_init(soft_i2c_t* soft_i2c);

// 发送一个起始信号
void soft_i2c_start(soft_i2c_t* soft_i2c);

// 发送一个停止信号
void soft_i2c_stop(soft_i2c_t* soft_i2c);

// 等待ACK
uint8_t soft_i2c_wait_ack(soft_i2c_t* soft_i2c);

// 发送ACK
void soft_i2c_ack(soft_i2c_t* soft_i2c);

// 发送NACK
void soft_i2c_nack(soft_i2c_t* soft_i2c);

// 在访问I2C设备前，请先调用 soft_i2c_check_device()
// 检测I2C设备是否正常，该函数会自动调用配置GPIO的函数
uint8_t soft_i2c_check_device(soft_i2c_t* soft_i2c, uint8_t address);

// 发送一个字节
u8 soft_i2c_send_byte(soft_i2c_t* soft_i2c, uint8_t byte);

// 读取一个字节
u8 soft_i2c_read_byte(soft_i2c_t* soft_i2c, uint8_t ack);

///////////////////////////////////////////////////////////////////////////////
// 高层接口，参考stm32的hal库的硬件i2c的参数含义。
// 目前只支持7位设备地址，现在的dev_addr要求左移一位，以包含读写位
vhil_state_type_t soft_i2c_master_write(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                        u32 timeout);
vhil_state_type_t soft_i2c_master_read(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                       u32 timeout);
vhil_state_type_t soft_i2c_slave_write(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                       u32 timeout);
vhil_state_type_t soft_i2c_slave_read(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                      u32 timeout);
vhil_state_type_t soft_i2c_mem_write(soft_i2c_t* soft_i2c, u16 dev_addr, u16 mem_addr,
                                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
vhil_state_type_t soft_i2c_mem_read(soft_i2c_t* soft_i2c, u16 dev_addr, u16 mem_addr,
                                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
vhil_state_type_t soft_i2c_is_device_ready(soft_i2c_t* soft_i2c, u16 dev_addr, u32 trials,
                                           u32 timeout);

#endif   // !MTLIB_DRIVER_SOFT_I2C_H
