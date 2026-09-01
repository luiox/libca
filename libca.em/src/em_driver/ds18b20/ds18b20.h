/// @file ds18b20.h
/// @author canrad
/// @brief DS18B20 温度传感器驱动（port 绑定风格）
/// @version 0.1
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
#ifndef LIBCA_EM_DRIVER_DS18B20_H
#define LIBCA_EM_DRIVER_DS18B20_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_DS18B20_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_DS18B20_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_DS18B20_PORT_MODE
#    define LIBCA_DS18B20_PORT_MODE LIBCA_DS18B20_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_DS18B20_PORT_MODE == LIBCA_DS18B20_PORT_MODE_EXTERN)
/// @brief 写引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚号
/// @param value 电平值
extern void port_ds18b20_write_pin(void* gpio, u16 pin, u8 value);
/// @brief 读引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚号
/// @return 当前电平值
extern u8 port_ds18b20_read_pin(void* gpio, u16 pin);
/// @brief 设置引脚为输出模式
/// @param gpio GPIO端口
/// @param pin 引脚号
extern void port_ds18b20_set_output_mode(void* gpio, u16 pin);
/// @brief 设置引脚为输入模式
/// @param gpio GPIO端口
/// @param pin 引脚号
extern void port_ds18b20_set_input_mode(void* gpio, u16 pin);
/// @brief 微秒延时
/// @param us 延时时间（微秒）
extern void port_ds18b20_delay_us(u32 us);

#elif (LIBCA_DS18B20_PORT_MODE == LIBCA_DS18B20_PORT_MODE_DYNAMIC)
typedef struct ds18b20_port
{
    void (*write_pin)(void* gpio, u16 pin, u8 value);   // 写引脚电平
    u8 (*read_pin)(void* gpio, u16 pin);                // 读引脚电平
    void (*set_output_mode)(void* gpio, u16 pin);       // 设置输出模式
    void (*set_input_mode)(void* gpio, u16 pin);        // 设置输入模式
    void (*delay_us)(u32 us);                           // 微秒延时
} ds18b20_port_t;
void ds18b20_bind_port(const ds18b20_port_t* port);
bool ds18b20_port_is_registered(void);

#else
#    error "Invalid DS18B20 port mode"
#endif

// 错误码定义（返回 i32）
#define DS18B20_OK 0
#define DS18B20_ERR_NO_PRESENCE (-1)           // 设备没有存在脉冲（presence pulse）
#define DS18B20_ERR_NO_RELEASE (-2)            // 设备没有释放脉冲（release pulse）
#define DS18B20_ERR_DEVICE_CHECK_FAILED (-3)   // 设备检测失败（通用）

typedef struct ds18b20
{
    void* gpio;
    u16   pin;
} ds18b20_t;

// 初始化设备（绑定 gpio 和 pin）
void ds18b20_init(ds18b20_t* self, void* gpio, u16 pin);
// 检查设备是否存在，返回 DS18B20_OK 表示存在，非 0 表示错误码
i32 ds18b20_check_device(ds18b20_t* self);
// 读取温度，返回 DS18B20_OK 表示成功，温度以原始 16 位值返回（单位为 1/16 °C）
i32 ds18b20_read_temperature(ds18b20_t* self, u16* temp);

#ifdef __cplusplus
}
#endif

#endif   // LIBCA_EM_DRIVER_DS18B20_H
