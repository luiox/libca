/// @file dht22.h
/// @author canrad (1517807724@qq.com)
/// @brief DHT22 温湿度传感器驱动
/// @version 0.1
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
///
/// @copyright Copyright (c) 2026
///
///  
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

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_DHT22_PORT_MODE == LIBCA_DHT22_PORT_MODE_EXTERN)
/// @brief 写引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚号
/// @param value 电平值
///  
extern void port_dht22_write_pin(void* gpio, u16 pin, u8 value);
/// @brief 读引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚号
/// @return 当前电平值
///  
extern u8 port_dht22_read_pin(void* gpio, u16 pin);
/// @brief 设置引脚为输出模式
/// @param gpio GPIO端口
/// @param pin 引脚号
///  
extern void port_dht22_set_output_mode(void* gpio, u16 pin);
/// @brief 设置引脚为输入模式
/// @param gpio GPIO端口
/// @param pin 引脚号
///  
extern void port_dht22_set_input_mode(void* gpio, u16 pin);
/// @brief 微秒延时
/// @param us 延时时间（微秒）
///  
extern void port_dht22_delay_us(u32 us);
/// @brief 毫秒延时
/// @param ms 延时时间（毫秒）
///  
extern void port_dht22_delay_ms(u32 ms);

#elif (LIBCA_DHT22_PORT_MODE == LIBCA_DHT22_PORT_MODE_DYNAMIC)
typedef struct dht22_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);  // 写引脚电平
    u8   (*read_pin)(void* gpio, u16 pin);              // 读引脚电平
    void (*set_output_mode)(void* gpio, u16 pin);       // 设置输出模式
    void (*set_input_mode)(void* gpio, u16 pin);        // 设置输入模式
    void (*delay_us)(u32 us);                           // 微秒延时
    void (*delay_ms)(u32 ms);                           // 毫秒延时
} dht22_port_t;
void dht22_bind_port(const dht22_port_t* port);
bool dht22_port_is_registered(void);

#else
#error "Invalid DHT22 port mode"
#endif

// 设备对象
typedef struct dht22 {
    void* gpio;
    u16   pin;
} dht22_t;

// 错误码（按规范：模块名_ERR_含义，负数表示错误）
#define DHT22_OK                       0
#define DHT22_ERR_NO_RESPONSE         (-1)
#define DHT22_ERR_BAD_ACK1            (-2)
#define DHT22_ERR_BAD_ACK2            (-3)
#define DHT22_ERR_TIMEOUT             (-4)
#define DHT22_ERR_CRC_FAIL            (-5)
#define DHT22_ERR_INVALID_PARAM       (-6)

// 初始化设备（绑定 gpio/pin）
void dht22_init(dht22_t* self, void* gpio, u16 pin);
// 读取一次测量结果，输出：humidity（单位 0.1%RH），temperature（单位 0.1°C，带符号）
// 返回 DHT22_OK 或 错误码
i32 dht22_read(dht22_t* self, u16* humidity10, i16* temperature10);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_DHT22_H