/// @file sgp30.h
/// @author Canrad
/// @brief SGP30 空气质量传感器驱动
/// @version 0.1
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_DRIVER_SGP30_H
#define LIBCA_EM_DRIVER_SGP30_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_SGP30_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_SGP30_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_SGP30_PORT_MODE
#    define LIBCA_SGP30_PORT_MODE LIBCA_SGP30_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_SGP30_PORT_MODE == LIBCA_SGP30_PORT_MODE_EXTERN)
/// @brief I2C写接口
/// @param hi2c I2C句柄
/// @param dev_addr 设备地址
/// @param mem_addr 寄存器地址
/// @param mem_addr_size 地址长度
/// @param data 数据缓冲
/// @param data_size 长度
/// @param timeout 超时
/// @return 0=成功
extern i32 port_sgp30_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                                u16 data_size, u32 timeout);
/// @brief I2C读接口
/// @param hi2c I2C句柄
/// @param dev_addr 设备地址
/// @param mem_addr 寄存器地址
/// @param mem_addr_size 地址长度
/// @param data 数据缓冲
/// @param data_size 长度
/// @param timeout 超时
/// @return 0=成功
extern i32 port_sgp30_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                               u16 data_size, u32 timeout);
/// @brief 毫秒延时
/// @param ms 延时时间
extern void port_sgp30_delay_ms(u32 ms);

#elif (LIBCA_SGP30_PORT_MODE == LIBCA_SGP30_PORT_MODE_DYNAMIC)
typedef struct sgp30_port
{
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);   // I2C写
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                    u16 data_size, u32 timeout);   // I2C读
    void (*delay_ms)(u32 ms);                      // 毫秒延时
} sgp30_port_t;
void sgp30_bind_port(const sgp30_port_t* port);
bool sgp30_port_is_registered(void);

#else
#    error "Invalid SGP30 port mode"
#endif

// 测量数据结构
typedef struct sgp30_data_st
{
    u16 co2;
    u16 tvoc;
} sgp30_data_t;

// 驱动对象
typedef struct sgp30
{
    void* hi2c;
} sgp30_t;

// 错误码
#define SGP30_OK 0
#define SGP30_ERR_I2C_FAIL (-1)
#define SGP30_ERR_CRC_FAIL (-2)

// 初始化与读取
void sgp30_init(sgp30_t* self, void* hi2c);
i32  sgp30_read(sgp30_t* self, sgp30_data_t* out);

#ifdef __cplusplus
}
#endif

#endif   // LIBCA_EM_DRIVER_SGP30_H
