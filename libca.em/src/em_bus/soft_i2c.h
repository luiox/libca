///
/// @file soft_i2c.h
/// @author Canrad
/// @brief 软件I2C
/// 注意目前暂时不支持从机模式的软件I2C
/// @version 1.3
/// @date 2024-05-12 原始移植
/// update
/// v1.2: 2025-08-1 修复delay错误
/// v1.3: 2026-01-29 重构为port机制实现
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_BUS_SOFT_I2C_H
#define LIBCA_EM_BUS_SOFT_I2C_H

#include <em_base/datatype.h>

typedef struct
{
    // 设置gpio模式
    void (*gpio_set_output_mode)(void* port, u16 pin);
    void (*gpio_set_input_mode)(void* port, u16 pin);
    // 读取高电平为1，读取低电平为0
    u8 (*gpio_read)(void* port, u16 pin);
    void (*gpio_write)(void* port, u16 pin, u8 value);
    void (*delay_us)(u32 us);
} soft_i2c_port_t;

void soft_i2c_bind_port(const soft_i2c_port_t* port);
bool soft_i2c_port_is_registered(void);

// 写控制bit
#ifndef I2C_WRITE
#    define I2C_WRITE 0
#endif
// 读控制bit
#ifndef I2C_READ
#    define I2C_READ 1
#endif

typedef struct
{
    // scl的pin信息
    void* scl_port;
    u16   scl_pin;
    // sda的pin信息
    void* sda_port;
    u16   sda_pin;
} soft_i2c_t;

void soft_i2c_init(soft_i2c_t* soft_i2c);

// 发送一个起始信号
void soft_i2c_start(soft_i2c_t* soft_i2c);

// 发送一个停止信号
void soft_i2c_stop(soft_i2c_t* soft_i2c);

// 等待ACK
u8 soft_i2c_wait_ack(soft_i2c_t* soft_i2c);

// 发送ACK
void soft_i2c_ack(soft_i2c_t* soft_i2c);

// 发送NACK
void soft_i2c_nack(soft_i2c_t* soft_i2c);

// 在访问I2C设备前，请先调用 soft_i2c_check_device()
// 检测I2C设备是否正常，该函数会自动调用配置GPIO的函数
u8 soft_i2c_check_device(soft_i2c_t* soft_i2c, u8 address);

// 发送一个字节
u8 soft_i2c_send_byte(soft_i2c_t* soft_i2c, u8 byte);

// 读取一个字节
u8 soft_i2c_read_byte(soft_i2c_t* soft_i2c, u8 ack);

///////////////////////////////////////////////////////////////////////////////
// 高层接口，参考stm32的hal库的硬件i2c的参数含义。
// 目前只支持7位设备地址，现在的dev_addr要求左移一位，以包含读写位
i32 soft_i2c_master_write(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size, u32 timeout);
i32 soft_i2c_master_read(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size, u32 timeout);
i32 soft_i2c_slave_write(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size, u32 timeout);
i32 soft_i2c_slave_read(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size, u32 timeout);
i32 soft_i2c_mem_write(soft_i2c_t* soft_i2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size,
                       u8* data, u16 data_size, u32 timeout);
i32 soft_i2c_mem_read(soft_i2c_t* soft_i2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                      u16 data_size, u32 timeout);
i32 soft_i2c_is_device_ready(soft_i2c_t* soft_i2c, u16 dev_addr, u32 trials, u32 timeout);

#endif   // !LIBCA_EM_BUS_SOFT_I2C_H
