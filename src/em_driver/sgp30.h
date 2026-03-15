/**
 * @file sgp30.h
 * @author canrad (1517807724@qq.com)
 * @brief SGP30 空气质量传感器驱动
 * @version 0.1
 * @date 2026-01-22
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_DRIVER_SGP30_H
#define LIBCA_EM_DRIVER_SGP30_H

#include <em_base/datatype.h>

// port
typedef struct sgp30_port
{
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                    u16 data_size, u32 timeout);
    void (*delay_ms)(u32 ms);
} sgp30_port_t;

void sgp30_bind_port(const sgp30_port_t* port);
bool sgp30_port_is_registered(void);

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
#define SGP30_ERR_PORT_NOT_REGISTERED (-1)
#define SGP30_ERR_I2C_FAIL (-2)
#define SGP30_ERR_CRC_FAIL (-3)

// 初始化与读取
void sgp30_init(sgp30_t* self, void* hi2c);
i32  sgp30_read(sgp30_t* self, sgp30_data_t* out);

#endif   // LIBCA_EM_DRIVER_SGP30_H
