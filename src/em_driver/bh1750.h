/**
 * @file bh1750.h
 * @author canrad (1517807724@qq.com)
 * @brief BH1750是一款数字型光照强度传感器
 * 此驱动实现对BH1750的驱动支持，参考文章：https://www.cnblogs.com/jefften/p/18613437
 * @version 0.1
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_BH1750_H
#define LIBCA_EM_DRIVER_BH1750_H

#include "../em_base/datatype.h"

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

// address(7 bit) + read or write(1 bit)
#define	BH1750_ADDR_WRITE	0x46
#define	BH1750_ADDR_READ	0x47
#define BH1750_I2C_HANDLE   hi2c1

typedef enum
{
    POWER_OFF_CMD	=	0x00,	// Power off
    POWER_ON_CMD	=	0x01,	// Power on
    RESET_REGISTER	=	0x07,	// Reset digital register
    CONT_H_MODE		=	0x10,	// Continuous high resolution mode, measurement time 120ms
    CONT_H_MODE2	=	0x11,	// Continuous high resolution mode2, measurement time 120ms
    CONT_L_MODE		=	0x13,	// Continuous low resolution mode, measurement time 16ms
    ONCE_H_MODE		=	0x20,	// Once high resolution mode, measurement time 120ms
    ONCE_H_MODE2	=	0x21,	// Once high resolution mode2, measurement time 120ms
    ONCE_L_MODE		=	0x23	// Once low resolution mode2, measurement time 120ms
} bh1750_mode_t;

typedef struct bh1750
{
    // 基础设备地址 (通常是 0b1010 xxx)后面的xxx根据不同型号自己确定
    u8 dev_addr_base;
    // i2c读写所需的hi2c句柄
    void* hi2c;
} bh1750_t;

void bh1750_init(bh1750_t* self);

// 错误码
#define BH1750_OK 0
#define BH1750_ERR_PORT_NOT_REGISTERED (-1)
#define BH1750_ERR_I2C_FAIL (-2)

i32 bh1750_start(bh1750_t* self, bh1750_mode_t mode);
i32 bh1750_read_lux(bh1750_t* self, u16 *lux);

#endif // !LIBCA_EM_DRIVER_BH1750_H
