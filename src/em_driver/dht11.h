/**
 * @file dht11.h
 * @author canrad (1517807724@qq.com)
 * @brief DHT11 温湿度传感器驱动
 * @version 0.1
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_DHT11_H
#define LIBCA_EM_DRIVER_DHT11_H

#include "../em_base/datatype.h"

// port
typedef struct dht11_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8   (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht11_port_t;

void dht11_bind_port(const dht11_port_t* port);
bool dht11_port_is_registered(void);

// 设备对象
typedef struct dht11 {
    void* gpio;
    u16   pin;
} dht11_t;

// 错误码
#define DHT11_OK                          0
#define DHT11_ERR_PORT_NOT_REGISTERED     (-1)
#define DHT11_ERR_NO_RESPONSE             (-2)
#define DHT11_ERR_BAD_ACK1                (-3)
#define DHT11_ERR_BAD_ACK2                (-4)
#define DHT11_ERR_TIMEOUT                 (-5)
#define DHT11_ERR_CHECKSUM_FAIL           (-6)
#define DHT11_ERR_INVALID_PARAM           (-7)

// 初始化（绑定 gpio/pin）
void dht11_init(dht11_t* self, void* gpio, u16 pin);
// 读取一次测量结果，输出：humidity（单位 0.1%RH），temperature（单位 0.1°C，带符号）
// 返回 DHT11_OK 或 错误码
i32 dht11_read(dht11_t* self, u16* humidity, i16* temperature);

#endif // LIBCA_EM_DRIVER_DHT11_H
