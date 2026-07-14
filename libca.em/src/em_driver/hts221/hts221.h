/// @file hts221.h
/// @author canrad (1517807724@qq.com)
/// @brief HTS221 温湿度传感器驱动
/// @version 0.1
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
/// 
/// @copyright Copyright (c) 2026
/// 
#ifndef LIBCA_EM_DRIVER_HTS221_H
#define LIBCA_EM_DRIVER_HTS221_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_HTS221_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_HTS221_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_HTS221_PORT_MODE
#define LIBCA_HTS221_PORT_MODE LIBCA_HTS221_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_HTS221_PORT_MODE == LIBCA_HTS221_PORT_MODE_EXTERN)
/// @brief I2C写寄存器
/// @param hi2c I2C句柄
/// @param dev_addr 设备地址
/// @param mem_addr 寄存器地址
/// @param mem_addr_size 地址长度
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时
/// @return 0=成功
extern i32 port_hts221_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
/// @brief I2C读寄存器
/// @param hi2c I2C句柄
/// @param dev_addr 设备地址
/// @param mem_addr 寄存器地址
/// @param mem_addr_size 地址长度
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时
/// @return 0=成功
extern i32 port_hts221_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
/// @brief 微秒延时
/// @param us 延时时间（微秒）
extern void port_hts221_delay_us(u32 us);

#elif (LIBCA_HTS221_PORT_MODE == LIBCA_HTS221_PORT_MODE_DYNAMIC)
typedef struct hts221_port {
    i32  (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);  // I2C写
    i32  (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);   // I2C读
    void (*delay_us)(u32 us);                                                                                              // 微秒延时
} hts221_port_t;
void hts221_bind_port(const hts221_port_t* port);
bool hts221_port_is_registered(void);

#else
#error "Invalid HTS221 port mode"
#endif

typedef struct hts221 {
    void* hi2c; // i2c 句柄
    u16   dev_addr; // 设备地址（8 位地址，例如 0xBE）
} hts221_t;

// 错误码
#define HTS221_OK                  0
#define HTS221_ERR_I2C_FAIL       (-1)
#define HTS221_ERR_INVALID_PARAM  (-2)

// 初始化（绑定 i2c 句柄与设备地址）
void hts221_init(hts221_t* self, void* hi2c, u16 dev_addr);
// 读取原始温度/湿度寄存器值（int16 原始值），返回 HTS221_OK 或 错误码
i32 hts221_read_raw_temperature(hts221_t* self, int16_t* temperature);
i32 hts221_read_raw_humidity(hts221_t* self, int16_t* humidity);
// 读取经过校准并换算后的温度（单位：0.1°C）与湿度（单位：0.1%RH）
i32 hts221_read_temperature(hts221_t* self, int16_t* temperature10);
i32 hts221_read_humidity(hts221_t* self, int16_t* humidity10);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_HTS221_H
