/**
 * @file max30102.h
 * @author GitHub Copilot
 * @brief MAX30102脉搏血氧仪和心率监测传感器的驱动
 * @version 0.1
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_MAX30102_H
#define LIBCA_EM_DRIVER_MAX30102_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

// port
typedef struct max30102_port {
    // i2c写函数
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                      u16 data_size, u32 timeout);
    // i2c读函数
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);
} max30102_port_t;

/**
 * @brief 绑定硬件接口
 * 
 * @param port 接口结构体
 */
void max30102_bind_port(const max30102_port_t* port);

/**
 * @brief 检查接口是否已注册
 * 
 * @return bool 是否已注册
 */
bool max30102_port_is_registered(void);

typedef struct max30102 {
    void* hi2c;
} max30102_t;

/**
 * @brief 初始化MAX30102
 * 
 * @param self 驱动对象
 * @param hi2c I2C句柄
 * @return bool 是否成功
 */
bool max30102_init(max30102_t* self, void* hi2c);

/**
 * @brief 读取FIFO数据
 * 
 * @param self 驱动对象
 * @param red_led 红光LED值
 * @param ir_led 红外LED值
 * @return bool 是否成功
 */
bool max30102_read_fifo(max30102_t* self, u32* red_led, u32* ir_led);

/**
 * @brief 写寄存器
 * 
 * @param self 驱动对象
 * @param addr 寄存器地址
 * @param data 数据
 * @return bool 是否成功
 */
bool max30102_write_reg(max30102_t* self, u8 addr, u8 data);

/**
 * @brief 读寄存器
 * 
 * @param self 驱动对象
 * @param addr 寄存器地址
 * @param data 数据指针
 * @return bool 是否成功
 */
bool max30102_read_reg(max30102_t* self, u8 addr, u8* data);

/**
 * @brief 复位MAX30102
 * 
 * @param self 驱动对象
 * @return bool 是否成功
 */
bool max30102_reset(max30102_t* self);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_MAX30102_H
