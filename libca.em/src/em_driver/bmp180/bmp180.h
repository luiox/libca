/// @file bmp180.h
/// @author Canrad
/// @brief BMP180 压力传感器驱动
/// 参考文章：https://blog.csdn.net/stmnnn/article/details/136875908
/// @version 0.1
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
///
/// @copyright Copyright (c) 2026
///
///
#ifndef LIBCA_EM_DRIVER_BMP180_H
#define LIBCA_EM_DRIVER_BMP180_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_BMP180_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_BMP180_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_BMP180_PORT_MODE
#    define LIBCA_BMP180_PORT_MODE LIBCA_BMP180_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_BMP180_PORT_MODE == LIBCA_BMP180_PORT_MODE_EXTERN)

/// @brief I2C 写操作
/// @param hi2c I2C 句柄
/// @param dev_addr 设备地址
/// @param mem_addr 内存地址
/// @param mem_addr_size 地址字节数
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时（ms）
/// @return 0 表示成功
///
extern i32 port_bmp180_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size,
                                 u8* data, u16 data_size, u32 timeout);

/// @brief I2C 读操作
/// @param hi2c I2C 句柄
/// @param dev_addr 设备地址
/// @param mem_addr 内存地址
/// @param mem_addr_size 地址字节数
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时（ms）
/// @return 0 表示成功
///
extern i32 port_bmp180_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                                u16 data_size, u32 timeout);

/// @brief 毫秒延时
/// @param ms 延时时间（ms）
///
extern void port_bmp180_delay_ms(u32 ms);

#elif (LIBCA_BMP180_PORT_MODE == LIBCA_BMP180_PORT_MODE_DYNAMIC)

typedef struct bmp180_port
{
    // i2c写函数
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);
    // i2c读函数
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                    u16 data_size, u32 timeout);
    // 毫秒延时函数
    void (*delay_ms)(u32 ms);
} bmp180_port_t;

void bmp180_bind_port(const bmp180_port_t* port);
bool bmp180_port_is_registered(void);

#else
#    error "Invalid BMP180 port mode"
#endif

// 错误码
#define BMP180_OK 0
#define BMP180_ERR_I2C_FAIL (-2)
#define BMP180_ERR_DEVICE_NOT_FOUND (-3)
#define BMP180_ERR_INVALID_PARAM (-4)

/// @brief BMP180 校准参数
///
typedef struct bmp180_calibration
{
    i16 AC1;
    i16 AC2;
    i16 AC3;
    u16 AC4;
    u16 AC5;
    u16 AC6;
    i16 B1;
    i16 B2;
    i16 MB;
    i16 MC;
    i16 MD;
    i32 B5;   // 计算中间量
} bmp180_calibration_t;

/// @brief BMP180 过采样率 (OSS)
///
typedef enum
{
    BMP180_OSS_ULOW_POWER = 0,   // 4.5ms
    BMP180_OSS_STANDARD   = 1,   // 7.5ms
    BMP180_OSS_HIGH_RES   = 2,   // 13.5ms
    BMP180_OSS_ULTRA_RES  = 3    // 25.5ms
} bmp180_oss_t;

/// @brief BMP180 设备实例
///
typedef struct bmp180
{
    void*                hi2c;       // I2C 句柄
    u16                  dev_addr;   // 设备地址 (8位)
    bmp180_calibration_t calib;      // 校准数据
} bmp180_t;

/// @brief 初始化 BMP180
/// @param self 实例指针
/// @param hi2c I2C 句柄
/// @param dev_addr 设备 8 位地址 (常用 0xEE)
/// @return i32 0 表示成功
///
i32 bmp180_init(bmp180_t* self, void* hi2c, u16 dev_addr);

/// @brief 软件复位
///
i32 bmp180_reset(bmp180_t* self);

/// @brief 检查设备是否存在 (读取 Chip ID)
///
i32 bmp180_check(bmp180_t* self);

/// @brief 读取原始温度 (UT)
///
i32 bmp180_read_raw_temp(bmp180_t* self, u16* ut);

/// @brief 读取原始压力 (UP)
///
i32 bmp180_read_raw_press(bmp180_t* self, bmp180_oss_t oss, u32* up);

/// @brief 读取并计算温度
/// @param temperature 结果单位: 摄氏度 (C)
///
i32 bmp180_read_temp(bmp180_t* self, f32* temperature);

/// @brief 读取并计算压力
/// @param pressure 结果单位: 帕斯卡 (Pa)
///
i32 bmp180_read_press(bmp180_t* self, bmp180_oss_t oss, i32* pressure);

/// @brief 同时读取温度和压力
///
i32 bmp180_read_all(bmp180_t* self, bmp180_oss_t oss, f32* temperature, i32* pressure);

/// @brief 压力单位转换: Pa -> mmHg
///
f32 bmp180_pa_to_mmhg(f32 pa);

/// @brief 根据压力计算海拔 (巴罗米特公式)
/// @param pa 当前压力 (Pa)
/// @return f32 估算海拔 (m)
///
f32 bmp180_pa_to_alt(f32 pa);

#ifdef __cplusplus
}
#endif

#endif   // LIBCA_EM_DRIVER_BMP180_H
