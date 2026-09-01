/// @file bh1750.h
/// @author Canrad
/// @brief BH1750是一款数字型光照强度传感器
/// 此驱动实现对BH1750的驱动支持，参考文章：https://www.cnblogs.com/jefften/p/18613437
/// @version 0.1
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
///
/// @copyright Copyright (c) 2026
///
///
#ifndef LIBCA_EM_DRIVER_BH1750_H
#define LIBCA_EM_DRIVER_BH1750_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_BH1750_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_BH1750_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_BH1750_PORT_MODE
#    define LIBCA_BH1750_PORT_MODE LIBCA_BH1750_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_BH1750_PORT_MODE == LIBCA_BH1750_PORT_MODE_EXTERN)

/// @brief I2C 写操作
/// @param hi2c I2C 句柄
/// @param dev_addr 设备地址
/// @param mem_addr 内存地址
/// @param mem_addr_size 地址字节数
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时（ms）
/// @return 0 表示成功，其他表示失败
///
extern i32 port_bh1750_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size,
                                 u8* data, u16 data_size, u32 timeout);

/// @brief I2C 读操作
/// @param hi2c I2C 句柄
/// @param dev_addr 设备地址
/// @param mem_addr 内存地址
/// @param mem_addr_size 地址字节数
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时（ms）
/// @return 0 表示成功，其他表示失败
///
extern i32 port_bh1750_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                                u16 data_size, u32 timeout);

#elif (LIBCA_BH1750_PORT_MODE == LIBCA_BH1750_PORT_MODE_DYNAMIC)

typedef struct bh1750_port
{
    // i2c写函数
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);
    // i2c读函数
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                    u16 data_size, u32 timeout);
} bh1750_port_t;

void bh1750_bind_port(const bh1750_port_t* port);
bool bh1750_port_is_registered(void);

#else
#    error "Invalid BH1750 port mode"
#endif

// 错误码
#define BH1750_OK 0
#define BH1750_ERR_I2C_FAIL (-2)

// address(7 bit) + read or write(1 bit)
#define BH1750_ADDR_WRITE 0x46
#define BH1750_ADDR_READ 0x47
#define BH1750_I2C_HANDLE hi2c1

typedef enum
{
    POWER_OFF_CMD  = 0x00,   // Power off
    POWER_ON_CMD   = 0x01,   // Power on
    RESET_REGISTER = 0x07,   // Reset digital register
    CONT_H_MODE    = 0x10,   // Continuous high resolution mode, measurement time 120ms
    CONT_H_MODE2   = 0x11,   // Continuous high resolution mode2, measurement time 120ms
    CONT_L_MODE    = 0x13,   // Continuous low resolution mode, measurement time 16ms
    ONCE_H_MODE    = 0x20,   // Once high resolution mode, measurement time 120ms
    ONCE_H_MODE2   = 0x21,   // Once high resolution mode2, measurement time 120ms
    ONCE_L_MODE    = 0x23    // Once low resolution mode2, measurement time 120ms
} bh1750_mode_t;

typedef struct bh1750
{
    // 基础设备地址 (通常是 0b1010 xxx)后面的xxx根据不同型号自己确定
    u8 dev_addr_base;
    // i2c读写所需的hi2c句柄
    void* hi2c;
} bh1750_t;

void bh1750_init(bh1750_t* self);

i32 bh1750_start(bh1750_t* self, bh1750_mode_t mode);
i32 bh1750_read_lux(bh1750_t* self, u16* lux);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_BH1750_H