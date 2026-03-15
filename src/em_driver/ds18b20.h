/**
 * @file ds18b20.h
 * @author canrad
 * @brief DS18B20 温度传感器驱动（port 绑定风格）
 * @version 0.1
 * @date 2026-01-22
 */
#ifndef LIBCA_EM_DRIVER_DS18B20_H
#define LIBCA_EM_DRIVER_DS18B20_H

#include "em_base/datatype.h"

typedef struct ds18b20_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8   (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    // 注入延时函数（微秒）
    void (*delay_us)(u32 us);
} ds18b20_port_t; 

void ds18b20_bind_port(const ds18b20_port_t* port);
bool ds18b20_port_is_registered(void);

typedef struct ds18b20 {
    void* gpio;
    u16   pin;
} ds18b20_t;

// 初始化设备（绑定 gpio 和 pin）
void ds18b20_init(ds18b20_t* self, void* gpio, u16 pin);

// 错误码定义（返回 i32）
#define DS18B20_OK                          0
#define DS18B20_ERR_PORT_NOT_REGISTERED    (-1)
#define DS18B20_ERR_NO_PRESENCE            (-2) // 设备没有存在脉冲（presence pulse）
#define DS18B20_ERR_NO_RELEASE             (-3) // 设备没有释放脉冲（release pulse）
#define DS18B20_ERR_DEVICE_CHECK_FAILED    (-4) // 设备检测失败（通用）

// 检查设备是否存在，返回 DS18B20_OK 表示存在，非 0 表示错误码
i32 ds18b20_check_device(ds18b20_t* self);
// 读取温度，返回 DS18B20_OK 表示成功，温度以原始 16 位值返回（单位为 1/16 °C）
i32 ds18b20_read_temperature(ds18b20_t* self, u16* temp);

#endif // LIBCA_EM_DRIVER_DS18B20_H
