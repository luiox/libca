/**
 * @file as5600.h
 * @author canrad (1517807724@qq.com)
 * @brief AS5600磁编码器驱动
 * @version 0.1
 * @date 2026-01-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_AS5600_H
#define LIBCA_EM_DRIVER_AS5600_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
#define AS5600_OK                          0
#define AS5600_ERR_PORT_NOT_REGISTERED    (-1)

// port
typedef struct as5600_port {
    /**
     * @brief I2C写函数
     * @param hi2c I2C句柄
     * @param dev_addr 设备地址(7位)
     * @param reg_addr 寄存器地址
     * @param data 数据指针
     * @param len 数据长度
     */
    void (*i2c_write)(void* hi2c, u8 dev_addr, u8 reg_addr, const u8* data, u16 len);
    
    /**
     * @brief I2C读函数
     * @param hi2c I2C句柄
     * @param dev_addr 设备地址(7位)
     * @param reg_addr 寄存器起始地址
     * @param data 接收缓冲区
     * @param len 数据长度
     */
    void (*i2c_read)(void* hi2c, u8 dev_addr, u8 reg_addr, u8* data, u16 len);
} as5600_port_t;

// 绑定port
void as5600_bind_port(const as5600_port_t* port);
bool as5600_port_is_registered(void);

typedef struct as5600 {
    void* hi2c;           // I2C句柄
} as5600_t;

/**
 * @brief 初始化AS5600驱动对象
 * 
 * @param self 对象指针
 * @param hi2c I2C句柄
 */
void as5600_init(as5600_t* self, void* hi2c);

/**
 * @brief 获取原始角度数值 (12位)
 * 
 * @param self 对象指针
 * @return u16 原始角度值 (0-4095)
 */
u16 as5600_read_raw_angle(as5600_t* self);

/**
 * @brief 原始角度转换为度数
 * 
 * @param angle 原始角度
 * @return f32 度数 (0.0 - 360.0)
 */
f32 as5600_raw_to_degree(u16 angle);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_DRIVER_AS5600_H
