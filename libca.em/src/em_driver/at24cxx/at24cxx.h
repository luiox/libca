/// @file at24cxx.h
/// @author canrad (1517807724@qq.com)
/// @brief at24cxx系列eeprom的驱动
/// 而且已经假定芯片的WP引脚已经接地，这样子不需要一个IO口控制开关读写保护。
/// 如果有需要读写保护控制的话，后期可以加上。
/// @version 0.2
/// @date 2025-08-12
/// @update 0.2 添加extern外部依赖注入模式
/// update 2026-01-11 适配新接口标准
///
/// @copyright Copyright (c) 2025
///
///  
#ifndef LIBCA_EM_DRIVER_AT24CXX_H
#define LIBCA_EM_DRIVER_AT24CXX_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_AT24CXX_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_AT24CXX_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_AT24CXX_PORT_MODE
#define LIBCA_AT24CXX_PORT_MODE LIBCA_AT24CXX_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_AT24CXX_PORT_MODE == LIBCA_AT24CXX_PORT_MODE_EXTERN)

/// @brief I2C 写操作
///
/// @param hi2c I2C 句柄
/// @param dev_addr 设备地址
/// @param mem_addr 内存地址
/// @param mem_addr_size 地址字节数
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时（ms）
///  
extern void port_at24cxx_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);

/// @brief I2C 读操作
///
/// @param hi2c I2C 句柄
/// @param dev_addr 设备地址
/// @param mem_addr 内存地址
/// @param mem_addr_size 地址字节数
/// @param data 数据缓冲区
/// @param data_size 数据长度
/// @param timeout 超时（ms）
///  
extern void port_at24cxx_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);

#elif (LIBCA_AT24CXX_PORT_MODE == LIBCA_AT24CXX_PORT_MODE_DYNAMIC)

typedef struct at24cxx_port
{
    // i2c写函数
    void (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                      u16 data_size, u32 timeout);
    // i2c读函数
    void (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                     u16 data_size, u32 timeout);
} at24cxx_port_t;

void at24cxx_bind_port(const at24cxx_port_t* port);
bool at24cxx_port_is_registered(void);

#else
#error "Invalid AT24CXX port mode"
#endif

typedef enum
{
    AT24C01,    // 128B
    AT24C02,    // 256B
    AT24C04,    // 512B
    AT24C08,    // 1KB
    AT24C16,    // 2KB
    AT24C32,    // 4KB
    AT24C64,    // 8KB
    AT24C128,   // 16KB
    AT24C256,   // 32KB
    AT24C512    // 64KB
} at24cxx_type_t;

typedef struct
{
    // EEPROM型号
    at24cxx_type_t type;
    // 基础设备地址 (通常是 0b1010 xxx)后面的xxx根据不同型号自己确定
    u8 dev_addr_base;
    // 总内存大小（字节）
    u16 mem_size;
    // 内存地址需要的字节数 (1或2)
    u8 mem_addr_bytes;
    // i2c读写所需的hi2c句柄
    void* hi2c;
} at24cxx_t;

/// @brief 初始化一个at2cxx
///
/// @param dev 设备结构体
/// @param type at24cxx类型
/// @param a0 硬件a0接的高低电平，高为1，低为0
/// @param a1 a1
/// @param a2 a2
/// @param hi2c i2c句柄
///  
void at24cxx_init(at24cxx_t* self, at24cxx_type_t type, u8 a0, u8 a1, u8 a2, void* hi2c);

/// @brief 写一个字节
///
/// @param self 设备结构体
/// @param addr 地址
/// @param data 数据
///  
void at24cxx_write_byte(at24cxx_t* self, u16 addr, u8 data);

/// @brief 读一个字节
///
/// @param self 设备结构体
/// @param addr 地址
/// @param u8 数据
///  
void at24cxx_read_byte(at24cxx_t* self, u16 addr, u8* data);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_AT24CXX_H