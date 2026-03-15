/**
 * @file as5600.h
 * @author canrad (1517807724@qq.com)
 * @brief AS5600磁编码器驱动
 * 数据手册：https://item.szlcsc.com/datasheet/AS5600-ASOT/511984.html
 * @version 0.1
 * @date 2026-01-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_AS5600_H
#define LIBCA_EM_DRIVER_AS5600_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_AS5600_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_AS5600_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_AS5600_PORT_MODE
#define LIBCA_AS5600_PORT_MODE LIBCA_AS5600_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
#define AS5600_OK                          0
#define AS5600_ERR_PORT_NOT_REGISTERED    (-1)

// 状态寄存器位定义
#define AS5600_STATUS_MH                (1 << 3)    // 磁场过强 (Magnet High)
#define AS5600_STATUS_ML                (1 << 4)    // 磁场过弱 (Magnet Low)
#define AS5600_STATUS_MD                (1 << 5)    // 检测到磁铁 (Magnet Detected)

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

#if (LIBCA_AS5600_PORT_MODE == LIBCA_AS5600_PORT_MODE_EXTERN)

/**
 * @brief 外部模式 I2C 写函数
 */
extern void port_as5600_i2c_write(void* hi2c, u8 dev_addr, u8 reg_addr, const u8* data, u16 len);

/**
 * @brief 外部模式 I2C 读函数
 */
extern void port_as5600_i2c_read(void* hi2c, u8 dev_addr, u8 reg_addr, u8* data, u16 len);

#elif (LIBCA_AS5600_PORT_MODE == LIBCA_AS5600_PORT_MODE_DYNAMIC)

// 绑定port
/**
 * @brief 显式模式下绑定硬件接口（动态注入）
 */
void as5600_bind_port(const as5600_port_t* port);
bool as5600_port_is_registered(void);

#else
#error "Invalid AS5600 port mode"
#endif

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
 * @brief 获取角度值 (度数 0.0 ~ 360.0)
 * 
 * @param self 对象指针
 * @return f32 角度
 */
f32 as5600_read_angle(as5600_t* self);

/**
 * @brief 辅助函数：原始角度转换为度数
 * 
 * @param angle 原始角度
 * @return f32 度数 (0.0 - 360.0)
 */
f32 as5600_raw_to_degree(u16 angle);

/**
 * @brief 获取传感器状态
 * 用于检查磁铁是否被检测到，以及磁场强度是否合适
 * @param self 对象指针
 * @return u8 状态寄存器值 (使用 AS5600_STATUS_xx 宏进行判断)
 */
u8 as5600_get_status(as5600_t* self);

/**
 * @brief 获取AGC(自动增益控制)值
 * 数值范围 0-255。理想情况下应在 128 左右。
 * 0 表示磁场极强，255 表示磁场极弱。
 * @param self 对象指针
 * @return u8 AGC值
 */
u8 as5600_get_agc(as5600_t* self);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_DRIVER_AS5600_H
