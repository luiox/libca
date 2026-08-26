///
/// @file soft_i2c_template_source.h
/// @author Canrad
/// @brief 软件iic驱动模板实现文件
/// @version 1.3
/// @date 2026-01-29
///
/// @copyright Copyright (c) 2026
///

#ifndef I2C_SCL_H
#error "Please define I2C_SCL_H macro before including soft_i2c_template_source.h"
#endif

#ifndef I2C_SCL_L
#error "Please define I2C_SCL_L macro before including soft_i2c_template_source.h"
#endif

#ifndef I2C_SDA_H
#error "Please define I2C_SDA_H macro before including soft_i2c_template_source.h"
#endif

#ifndef I2C_SDA_L
#error "Please define I2C_SDA_L macro before including soft_i2c_template_source.h"
#endif

#ifndef I2C_SDA_READ
#error "Please define I2C_SDA_READ macro before including soft_i2c_template_source.h"
#endif

#ifndef I2C_SDA_OUT
#error "Please define I2C_SDA_OUT macro before including soft_i2c_template_source.h"
#endif

#ifndef I2C_SDA_IN
#error "Please define I2C_SDA_IN macro before including soft_i2c_template_source.h"
#endif

#ifndef I2C_DELAY
#error "Please define I2C_DELAY macro before including soft_i2c_template_source.h"
#endif

#ifndef SOFT_I2C_WR
#define SOFT_I2C_WR 0
#endif

#ifndef SOFT_I2C_RD
#define SOFT_I2C_RD 1
#endif

// 发起I2C总开始信号
void I2C_CLASS_FUNC_NAME(start)(void)
{
    I2C_SDA_OUT();
    I2C_SDA_H();
    I2C_SCL_H();
    I2C_DELAY();
    I2C_SDA_L();
    I2C_DELAY();
    I2C_SCL_L();
    I2C_DELAY();
}

// 发起I2C总线停止信号
void I2C_CLASS_FUNC_NAME(stop)(void)
{
    I2C_SDA_OUT();
    I2C_SDA_L();
    I2C_SCL_H();
    I2C_DELAY();
    I2C_SDA_H();
    I2C_DELAY();
}

// 向I2C总线设备发送8bit数据
void I2C_CLASS_FUNC_NAME(send_byte)(u8 byte)
{
    u8 i;
    I2C_SDA_OUT();
    I2C_SCL_L();
    for (i = 0; i < 8; i++) {
        if (byte & 0x80) {
            I2C_SDA_H();
        } else {
            I2C_SDA_L();
        }
        byte <<= 1;
        I2C_DELAY();
        I2C_SCL_H();
        I2C_DELAY();
        I2C_SCL_L();
        I2C_DELAY();
    }
}

// CPU从I2C总线设备读取8bit数据
u8 I2C_CLASS_FUNC_NAME(read_byte)(void)
{
    u8 i, data = 0;
    I2C_SDA_IN();
    for (i = 0; i < 8; i++) {
        data <<= 1;
        I2C_SCL_H();
        I2C_DELAY();
        if (I2C_SDA_READ()) {
            data++;
        }
        I2C_SCL_L();
        I2C_DELAY();
    }
    return data;
}

// CPU产生一个时钟，并读取器件的ACK应答信号
u8 I2C_CLASS_FUNC_NAME(wait_ack)(void)
{
    u8 timeout = 0;
    I2C_SDA_H();
    I2C_SDA_IN();
    I2C_DELAY();
    I2C_SCL_H();
    I2C_DELAY();
    while (I2C_SDA_READ()) {
        timeout++;
        if (timeout > 100) {
            I2C_CLASS_FUNC_NAME(stop)();
            return 1;
        }
    }
    I2C_SCL_L();
    I2C_DELAY();
    return 0;
}

// CPU产生一个ACK信号
void I2C_CLASS_FUNC_NAME(ack)(void)
{
    I2C_SCL_L();
    I2C_SDA_OUT();
    I2C_SDA_L();
    I2C_DELAY();
    I2C_SCL_H();
    I2C_DELAY();
    I2C_SCL_L();
    I2C_DELAY();
    I2C_SDA_H();
}

// CPU产生1个NACK信号
void I2C_CLASS_FUNC_NAME(nack)(void)
{
    I2C_SCL_L();
    I2C_SDA_OUT();
    I2C_SDA_H();
    I2C_DELAY();
    I2C_SCL_H();
    I2C_DELAY();
    I2C_SCL_L();
    I2C_DELAY();
}

// 初始化I2C
void I2C_CLASS_FUNC_NAME(init)(void)
{
    // 给一个停止信号, 复位I2C总线上的所有设备到待机模式
    I2C_CLASS_FUNC_NAME(stop)();
}

// 检测I2C总线设备
u8 I2C_CLASS_FUNC_NAME(check_device)(u8 address)
{
    u8 ack;
    I2C_CLASS_FUNC_NAME(start)();
    I2C_CLASS_FUNC_NAME(send_byte)(address | SOFT_I2C_WR);
    ack = I2C_CLASS_FUNC_NAME(wait_ack)();
    I2C_CLASS_FUNC_NAME(stop)();
    return ack;
}

///////////////////////////////////////////////////////////////////////////////
// 完成后清理宏定义，允许再次包含用于生成不同的 I2C 实例

#ifdef USER_I2C_PREFIX_NAME
#undef USER_I2C_PREFIX_NAME
#endif

#ifdef NAME_CONNECT
#undef NAME_CONNECT
#endif

#ifdef I2C_CLASS_NAME
#undef I2C_CLASS_NAME
#endif

#ifdef I2C_CLASS_FUNC_NAME
#undef I2C_CLASS_FUNC_NAME
#endif

#ifdef I2C_SCL_H
#undef I2C_SCL_H
#endif

#ifdef I2C_SCL_L
#undef I2C_SCL_L
#endif

#ifdef I2C_SDA_H
#undef I2C_SDA_H
#endif

#ifdef I2C_SDA_L
#undef I2C_SDA_L
#endif

#ifdef I2C_SDA_READ
#undef I2C_SDA_READ
#endif

#ifdef I2C_SDA_OUT
#undef I2C_SDA_OUT
#endif

#ifdef I2C_SDA_IN
#undef I2C_SDA_IN
#endif

#ifdef I2C_DELAY
#undef I2C_DELAY
#endif

#ifdef SOFT_I2C_WR
#undef SOFT_I2C_WR
#endif

#ifdef SOFT_I2C_RD
#undef SOFT_I2C_RD
#endif

