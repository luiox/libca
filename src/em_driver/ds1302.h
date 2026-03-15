/**
 * @file ds1302.h
 * @author canrad (1517807724@qq.com)
 * @brief ds1302 时钟芯片驱动
 * 参考文章：https://blog.csdn.net/qq_43270506/article/details/104952746
 * @version 0.1
 * @date 2026-01-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_DS1302_H
#define LIBCA_EM_DRIVER_DS1302_H

#include "em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

// port
typedef struct ds1302_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8 (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
} ds1302_port_t;

/**
 * @brief 绑定底层的硬件接口
 * 
 * @param port 硬件接口
 */
void ds1302_bind_port(const ds1302_port_t* port);

/**
 * @brief 检查硬件接口是否注册
 * 
 * @return true 
 * @return false 
 */
bool ds1302_port_is_registered(void);

/**
 * @brief 时间数据结构体
 * 顺序与 DS1302 芯片寄存器顺序一致 (秒, 分, 时, 日, 月, 周, 年)
 */
typedef struct ds1302_time {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u8 week;
    u16 year;
} ds1302_time_t;

/**
 * @brief DS1302 驱动对象
 * 
 */
typedef struct ds1302 {
    void* ce_port;
    u16 ce_pin;
    void* sclk_port;
    u16 sclk_pin;
    void* data_port;
    u16 data_pin;
} ds1302_t;

/**
 * @brief DS1302 初始化
 * 
 * @param self 驱动对象
 */
void ds1302_init(ds1302_t* self);

/**
 * @brief 向DS1302发送一字节数据
 * 
 * @param self 驱动对象
 * @param data 数据
 */
void ds1302_write_byte(ds1302_t* self, u8 data);

/**
 * @brief 向指定寄存器写一字节数据
 * 
 * @param self 驱动对象
 * @param address 寄存器地址
 * @param data 数据
 */
void ds1302_write_reg(ds1302_t* self, u8 address, u8 data);

/**
 * @brief 从指定寄存器读一字节数据
 * 
 * @param self 驱动对象
 * @param address 寄存器地址
 * @return u8 读取的数据
 */
u8 ds1302_read_reg(ds1302_t* self, u8 address);

/**
 * @brief 从DS1302读取实时时间
 * 
 * @param self 驱动对象
 * @param time 存储的时间数据
 */
void ds1302_get_time(ds1302_t* self, ds1302_time_t* time);

/**
 * @brief 设置DS1302的实时时间
 * 
 * @param self 驱动对象
 * @param time 要设置的时间数据
 */
void ds1302_set_time(ds1302_t* self, const ds1302_time_t* time);

/**
 * @brief 使用突发模式从DS1302读取实时时间
 * 
 * @param self 驱动对象
 * @param time 存储的时间数据
 */
void ds1302_get_time_fast(ds1302_t* self, ds1302_time_t* time);

/**
 * @brief 使用突发模式设置DS1302的实时时间
 * 
 * @param self 驱动对象
 * @param time 要设置的时间数据
 */
void ds1302_set_time_fast(ds1302_t* self, const ds1302_time_t* time);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_DRIVER_DS1302_H
