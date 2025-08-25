/**
 * @file resource_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 资源管理的辅助工具，通过绑定机制，实现隔离硬件资源和驱动程序
 * @version 0.1
 * @date 2025-08-03
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef RESOURCE_UTIL_H
#define RESOURCE_UTIL_H

#include "../base/datatype.h" 

#define RESOURCE_MAX_SIZE 0xFF

typedef usize resource_id_t;

/**
 * @brief 绑定资源
 *
 * @param id 资源id
 * @param res 资源指针
 */
void resource_bind(resource_id_t id, void* res);

/**
 * @brief 获取资源
 *
 * @param id 资源id
 * @return void* 资源指针
 */
void* resource_get(resource_id_t id);

/**
 * @brief 获取资源并转换类型
 *
 * @param id 源资源id
 * @param type 目标资源类型
 * @return type* 目标资源指针
 */
#define resource_get_as(id, type) ((type*)resource_get(id))

// GPIO操作接口
u8 gpio_read_pin(resource_id_t id);
void gpio_write_pin(resource_id_t id, u8 value);
void gpio_toggle_pin(resource_id_t id);


// 暂定
// // UART操作接口
// typedef struct {
//     void (*init)(void* handle);
//     void (*send_byte)(void* handle, u8 data);
//     u8 (*receive_byte)(void* handle);
//     usize (*send_buffer)(void* handle, const u8* buffer, usize size);
//     usize (*receive_buffer)(void* handle, u8* buffer, usize size);
// } uart_interface_t;

// // SPI操作接口
// typedef struct {
//     void (*init)(void* handle);
//     u8 (*transfer)(void* handle, u8 data);
//     void (*transfer_buffer)(void* handle, const u8* tx_buffer, u8* rx_buffer, usize size);
// } spi_interface_t;

// // I2C操作接口
// typedef struct {
//     void (*init)(void* handle);
//     bool (*master_transmit)(void* handle, u8 addr, const u8* data, usize size);
//     bool (*master_receive)(void* handle, u8 addr, u8* data, usize size);
// } i2c_interface_t;

// // 定时器接口
// typedef struct {
//     void (*init)(void* handle);
//     void (*start)(void* handle);
//     void (*stop)(void* handle);
//     u32 (*get_tick)(void* handle);
// } timer_interface_t;

// // ADC接口
// typedef struct {
//     void (*init)(void* handle);
//     void (*start)(void* handle);
//     u16 (*read_value)(void* handle);
// } adc_interface_t;

// // PWM接口
// typedef struct {
//     void (*init)(void* handle);
//     void (*set_duty_cycle)(void* handle, u32 duty_cycle);
//     void (*set_frequency)(void* handle, u32 frequency);
// } pwm_interface_t;

// 低电平有效
#define VALID_ON_LOW_LEVEL 0
// 高电平有效
#define VALID_ON_HIGH_LEVEL 1

#endif
