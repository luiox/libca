 /// @file w25qxx.h
 /// @author canrad (1517807724@qq.com)
 /// @brief w25qxx系列spi flash驱动 已实物验证w25q64模块
 /// @version 0.1
 /// @date 2026-01-11
 /// @update 0.2 添加extern外部依赖注入模式
 ///
 /// @copyright Copyright (c) 2026
 ///
#ifndef LIBCA_EM_DRIVER_W25QXX_H
#define LIBCA_EM_DRIVER_W25QXX_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_W25QXX_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_W25QXX_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_W25QXX_PORT_MODE
#define LIBCA_W25QXX_PORT_MODE LIBCA_W25QXX_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_W25QXX_PORT_MODE == LIBCA_W25QXX_PORT_MODE_EXTERN)
 /// @brief 写引脚电平
 /// @param gpio_port GPIO端口
 /// @param pin 引脚编号
 /// @param value 电平值
extern void port_w25qxx_write_pin(void* gpio_port, u16 pin, u8 value);
 /// @brief SPI发送
 /// @param hspi SPI句柄
 /// @param data 数据缓冲
 /// @param size 字节数
 /// @param timeout 超时
extern void port_w25qxx_spi_transmit(void* hspi, u8* data, usize size, u32 timeout);
 /// @brief SPI接收
 /// @param hspi SPI句柄
 /// @param data 数据缓冲
 /// @param size 字节数
 /// @param timeout 超时
extern void port_w25qxx_spi_receive(void* hspi, u8* data, usize size, u32 timeout);
 /// @brief SPI收发
 /// @param hspi SPI句柄
 /// @param tx_data 发送缓冲
 /// @param rx_data 接收缓冲
 /// @param size 字节数
 /// @param timeout 超时
extern void port_w25qxx_spi_transmit_receive(void* hspi, u8* tx_data, u8* rx_data, usize size, u32 timeout);

#elif (LIBCA_W25QXX_PORT_MODE == LIBCA_W25QXX_PORT_MODE_DYNAMIC)
typedef struct w25qxx_port {
    void (*write_pin)(void* gpio_port, u16 pin, u8 value);                                                  // 写引脚电平
    void (*spi_transmit)(void* hspi, u8* data, usize size, u32 timeout);                                    // SPI发送
    void (*spi_receive)(void* hspi, u8* data, usize size, u32 timeout);                                     // SPI接收
    void (*spi_transmit_receive)(void* hspi, u8* tx_data, u8* rx_data, usize size, u32 timeout);           // SPI收发
} w25qxx_port_t;
void w25qxx_bind_port(const w25qxx_port_t* port);
bool w25qxx_port_is_registered(void);

#else
#error "Invalid W25QXX port mode"
#endif

// 错误码
#define W25QXX_OK 0
#define W25QXX_ERR_SPI_FAIL (-1)

typedef struct w25qxx {
    void* hspi;
    void* cs_gpio;
    u16   cs_pin;
}w25qxx_t;

// 使用之前需要初始化好SPI和CS引脚，这个函数负责让模块绑定SPI和CS引脚
void w25qxx_init(w25qxx_t* self);
// 读取设备ID
u16 w25qxx_read_device_id(w25qxx_t* self);
// 使能写入
void w25qxx_write_enable(w25qxx_t* self);
// 禁用写入
void w25qxx4_write_disable(w25qxx_t* self);
// 读取状态寄存器
u8 w25qxx_read_status_register(w25qxx_t* self);
// 擦除扇区
void w25qxx_sector_erase(w25qxx_t* self,u32 address);
// 读取数据
void w25qxx_read_data(w25qxx_t* self,u32 address,uint8_t *buf, usize len);
// 扇区写入数据，一次最多写入256字节
void w25qxx_page_program(w25qxx_t* self,u32 address,uint8_t *buf, usize len);

#ifdef __cplusplus
}
#endif


#endif   // !LIBCA_EM_DRIVER_W25QXX_H
