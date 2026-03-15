/**
 * @file dht22.h
 * @author canrad (1517807724@qq.com)
 * @brief DHT22 温湿度传感器驱动
 * @version 0.1
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_DHT22_H
#define LIBCA_EM_DRIVER_DHT22_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_DHT22_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_DHT22_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_DHT22_PORT_MODE
#define LIBCA_DHT22_PORT_MODE LIBCA_DHT22_PORT_MODE_EXTERN
#endif

// port 定义：抽象 GPIO 操作和延时
typedef struct dht22_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8   (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht22_port_t;
extern void port_dht22_write_pin(void* gpio, u16 pin, u8 value);
extern u8 port_dht22_read_pin(void* gpio, u16 pin);
extern void port_dht22_set_output_mode(void* gpio, u16 pin);
extern void port_dht22_set_input_mode(void* gpio, u16 pin);
extern void port_dht22_delay_us(u32 us);
extern void port_dht22_delay_ms(u32 ms);

void dht22_bind_port(const dht22_port_t* port);
bool dht22_port_is_registered(void);

// 设备对象
typedef struct dht22 {
    void* gpio;
    u16   pin;
} dht22_t;

// 错误码（按规范：模块名_ERR_含义，负数表示错误）
#define DHT22_OK                       0
#define DHT22_ERR_PORT_NOT_REGISTERED (-1)
#define DHT22_ERR_NO_RESPONSE         (-2)
#define DHT22_ERR_BAD_ACK1            (-3)
#define DHT22_ERR_BAD_ACK2            (-4)
#define DHT22_ERR_TIMEOUT             (-5)
#define DHT22_ERR_CRC_FAIL            (-6)
#define DHT22_ERR_INVALID_PARAM      (-7)

// 初始化设备（绑定 gpio/pin）
void dht22_init(dht22_t* self, void* gpio, u16 pin);
// 读取一次测量结果，输出：humidity（单位 0.1%RH），temperature（单位 0.1°C，带符号）
// 返回 DHT22_OK 或 错误码
i32 dht22_read(dht22_t* self, u16* humidity10, i16* temperature10);

#endif // LIBCA_EM_DRIVER_DHT22_H
