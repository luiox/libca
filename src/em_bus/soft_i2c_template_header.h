/**
 * @file soft_i2c_template_header.h
 * @author canrad (1517807724@qq.com)
 * @brief 软件iic驱动模板头文件
 * 
 * 使用方法：
 * 1. 在头文件包含之前soft_i2c_template_header.h 定义 USER_I2C_PREFIX_NAME 宏（可选，默认为 soft）
 * 2. 在源文件包含soft_i2c_template_source.h之前先再次定义USER_I2C_PREFIX_NAME，要跟头文件的一模一样。
 *    并且定义底层操作宏：
 *    - I2C_SCL_H()      : 拉高 SCL
 *    - I2C_SCL_L()      : 拉低 SCL
 *    - I2C_SDA_H()      : 拉高 SDA
 *    - I2C_SDA_L()      : 拉低 SDA
 *    - I2C_SDA_READ()   : 读取 SDA 电平 (返回 0 或 1)
 *    - I2C_SDA_OUT()    : 设置 SDA 为输出方向
 *    - I2C_SDA_IN()     : 设置 SDA 为输入方向
 *    - I2C_DELAY()      : 延时宏 (控制 I2C 速率)
 * 3. 包含此头文件生成函数声明
 * 4. 包含 soft_i2c_template_source.h 生成函数实现
 * 
 * @version 1.3
 * @date 2026-01-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "../em_base/datatype.h"

// 如果没有就用这个
#ifndef USER_I2C_PREFIX_NAME
#define USER_I2C_PREFIX_NAME soft
#endif

#ifndef NAME_CONNECT
#define NAME_CONNECT(a, b) a##b
#endif

#ifndef I2C_CLASS_NAME
#define I2C_CLASS_NAME NAME_CONNECT(USER_I2C_PREFIX_NAME, _i2c_)
#endif

#ifndef I2C_CLASS_FUNC_NAME
#define I2C_CLASS_FUNC_NAME(func) NAME_CONNECT(I2C_CLASS_NAME, func)
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief 初始化I2C总线
 */
void I2C_CLASS_FUNC_NAME(init)(void);

/**
 * @brief 检测I2C总线上的设备是否存在
 * @param address 设备地址
 * @return u8 0: 存在, 1: 不存在
 */
u8 I2C_CLASS_FUNC_NAME(check_device)(u8 address);

/**
 * @brief 发送起始信号
 */
void I2C_CLASS_FUNC_NAME(start)(void);

/**
 * @brief 发送停止信号
 */
void I2C_CLASS_FUNC_NAME(stop)(void);

/**
 * @brief 等待应答信号
 * @return u8 0: 收到应答, 1: 无应答
 */
u8 I2C_CLASS_FUNC_NAME(wait_ack)(void);

/**
 * @brief 发送应答信号
 */
void I2C_CLASS_FUNC_NAME(ack)(void);

/**
 * @brief 发送非应答信号
 */
void I2C_CLASS_FUNC_NAME(nack)(void);

/**
 * @brief 发送一个字节
 * @param byte 要发送的数据
 */
void I2C_CLASS_FUNC_NAME(send_byte)(u8 byte);

/**
 * @brief 读取一个字节
 * @return u8 读取到的数据
 */
u8 I2C_CLASS_FUNC_NAME(read_byte)(void);

///////////////////////////////////////////////////////////////////////////////

// 生成完以后取消宏定义
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

// 使用例子
#if 0
// 头文件my_i2c_device.h内
#ifndef MY_I2C_DEVICE_H
#define MY_I2C_DEVICE_H

#define USER_I2C_PREFIX_NAME my_dev
#include "soft_i2c_template_header.h"

#endif // !MY_I2C_DEVICE_H

// 源文件my_i2c_device.c内
// 先包含头文件
#include "my_i2c_device.h"

// 修改为针对平台的不同实现

// 假设我们用的是HAL库
// PA9是SCL，PA10是SDA

// 设置SCL为高电平
static inline void i2c_scl_high(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
}
// 设置SCL为低电平
static inline void i2c_scl_low(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
}
// 设置SDA为高电平
static inline void i2c_sda_high(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
}
// 设置SDA为低电平
static inline void i2c_sda_low(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
}
// 设置SDA为输出模式
static inline void i2c_sda_out(void)
{
    // 设置GPIOA的第10号引脚为输出模式
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
// 设置SDA为输入模式
static inline void i2c_sda_in(void)
{
    // 设置GPIOA的第10号引脚为输入模式
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
// 读取SDA的电平
static inline u8 i2c_sda_read(void)
{
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET ? 1 : 0;
}
// I2C总线位延迟，最快400KHz
static void i2c_delay(void)
{
    volatile u8 i;

    for (i = 0; i < 5; i++);
}

// 定义所需的宏
#define I2C_SCL_H i2c_scl_high
#define I2C_SCL_L i2c_scl_low
#define I2C_SDA_H i2c_sda_high
#define I2C_SDA_L i2c_sda_low
#define I2C_SDA_READ i2c_sda_read
#define I2C_SDA_OUT i2c_sda_out
#define I2C_SDA_IN i2c_sda_in
#define I2C_DELAY i2c_delay

// 然后包含生成实现的头文件
#include "soft_i2c_template_source.h"

#endif
