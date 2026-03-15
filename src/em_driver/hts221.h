/**
 * @file hts221.h
 * @author canrad (1517807724@qq.com)
 * @brief HTS221 温湿度传感器驱动
 * @version 0.1
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_HTS221_H
#define LIBCA_EM_DRIVER_HTS221_H

#include <em_base/datatype.h>

// port
typedef struct hts221_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    void (*delay_us)(u32 us);
} hts221_port_t;

void hts221_bind_port(const hts221_port_t* port);
bool hts221_port_is_registered(void);

typedef struct hts221 {
    void* hi2c; // i2c 句柄
    u16   dev_addr; // 设备地址（8 位地址，例如 0xBE）
} hts221_t;

// 错误码
#define HTS221_OK 0
#define HTS221_ERR_PORT_NOT_REGISTERED (-1)
#define HTS221_ERR_I2C_FAIL (-2)
#define HTS221_ERR_INVALID_PARAM (-3)

// 初始化（绑定 i2c 句柄与设备地址）
void hts221_init(hts221_t* self, void* hi2c, u16 dev_addr);
// 读取原始温度/湿度寄存器值（int16 原始值），返回 HTS221_OK 或 错误码
i32 hts221_read_raw_temperature(hts221_t* self, int16_t* temperature);
i32 hts221_read_raw_humidity(hts221_t* self, int16_t* humidity);
// 读取经过校准并换算后的温度（单位：0.1°C）与湿度（单位：0.1%RH）
i32 hts221_read_temperature(hts221_t* self, int16_t* temperature10);
i32 hts221_read_humidity(hts221_t* self, int16_t* humidity10);

#endif // LIBCA_EM_DRIVER_HTS221_H
