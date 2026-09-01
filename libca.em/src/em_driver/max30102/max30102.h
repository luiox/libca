/// @file max30102.h
/// @author GitHub Copilot
/// @brief MAX30102脉搏血氧仪和心率监测传感器的驱动
/// @version 0.2
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_DRIVER_MAX30102_H
#define LIBCA_EM_DRIVER_MAX30102_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_MAX30102_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_MAX30102_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_MAX30102_PORT_MODE
#    define LIBCA_MAX30102_PORT_MODE LIBCA_MAX30102_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX30102_ADDR 0xAE
#define MAX30102_BUFFER_SIZE 500

#if (LIBCA_MAX30102_PORT_MODE == LIBCA_MAX30102_PORT_MODE_EXTERN)
/// @brief I2C写寄存器
/// @param hi2c I2C句柄
/// @param dev_addr 设备地址
/// @param mem_addr 寄存器地址
/// @param mem_addr_size 地址长度
/// @param data 数据
/// @param data_size 数据长度
/// @param timeout 超时
/// @return 0=成功
extern i32 port_max30102_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size,
                                   u8* data, u16 data_size, u32 timeout);
/// @brief I2C读寄存器
/// @param hi2c I2C句柄
/// @param dev_addr 设备地址
/// @param mem_addr 寄存器地址
/// @param mem_addr_size 地址长度
/// @param data 数据
/// @param data_size 数据长度
/// @param timeout 超时
/// @return 0=成功
extern i32 port_max30102_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size,
                                  u8* data, u16 data_size, u32 timeout);

#elif (LIBCA_MAX30102_PORT_MODE == LIBCA_MAX30102_PORT_MODE_DYNAMIC)
typedef struct max30102_port
{
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);   // I2C写
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                    u16 data_size, u32 timeout);   // I2C读
} max30102_port_t;
void max30102_bind_port(const max30102_port_t* port);
bool max30102_port_is_registered(void);

#else
#    error "Invalid MAX30102 port mode"
#endif

typedef struct max30102_data
{
    i32  heart_rate;         ///< 心率值
    bool heart_rate_valid;   ///< 心率值是否有效
    i32  spo2;               ///< 血氧饱和度
    bool spo2_valid;         ///< 血氧饱和度是否有效
} max30102_data_t;

typedef struct max30102
{
    void* hi2c;
    i32*  an_dx_buf;
    usize an_dx_buf_size;
    i32*  an_x_buf;
    usize an_x_buf_size;
    i32*  an_y_buf;
    usize an_y_buf_size;
} max30102_t;

/// @brief 初始化MAX30102
///
/// @param self 驱动对象
/// @param hi2c I2C句柄
/// @param dx_buf 临时缓冲区dx
/// @param dx_size dx缓冲区大小
/// @param x_buf 临时缓冲区x
/// @param x_size x缓冲区大小
/// @param y_buf 临时缓冲区y
/// @param y_size y缓冲区大小
/// @return bool 是否成功
bool max30102_init(max30102_t* self, void* hi2c, i32* dx_buf, usize dx_size, i32* x_buf,
                   usize x_size, i32* y_buf, usize y_size);

/// @brief 读取单次FIFO数据 (红光和红外)
///
/// @param self 驱动对象
/// @param red_led 红光LED值存储指针
/// @param ir_led 红外LED值存储指针
/// @return bool 是否成功
bool max30102_read_fifo(max30102_t* self, u32* red_led, u32* ir_led);

/// @brief 计算心率和血氧饱和度
/// 基于样本进行计算
///
/// @param self 驱动对象
/// @param ir_buffer 红外数据样本
/// @param red_buffer 红光数据样本
/// @param result 计算结果
void max30102_calculate(max30102_t* self, u32* ir_buffer, u32* red_buffer, max30102_data_t* result);

/// @brief 复位MAX30102
///
/// @param self 驱动对象
/// @return bool 是否成功
bool max30102_reset(max30102_t* self);

#ifdef __cplusplus
}
#endif

#endif   // LIBCA_EM_DRIVER_MAX30102_H
