/**
 * @file lcd1602.h
 * @author canrad (1517807724@qq.com)
 * @brief LCD1602 液晶显示屏驱动 (支持4线和8线模式)
 * @version 0.1
 * @date 2026-01-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_LCD1602_H
#define LIBCA_EM_DRIVER_LCD1602_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

// LCD1602 模式枚举
typedef enum lcd1602_mode_enum {
    LCD1602_MODE_4BIT = 0,
    LCD1602_MODE_8BIT = 1,
} lcd1602_mode_t;

// port
typedef struct lcd1602_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} lcd1602_port_t;

/**
 * @brief 绑定底层的硬件接口
 * 
 * @param port 硬件接口
 */
void lcd1602_bind_port(const lcd1602_port_t* port);

/**
 * @brief 检查硬件接口是否注册
 * 
 * @return true 
 * @return false 
 */
bool lcd1602_port_is_registered(void);

/**
 * @brief LCD1602 驱动对象
 * 
 */
typedef struct lcd1602 {
    lcd1602_mode_t mode;      // 模式：4线或8线
    void* rs_port;          // RS 引脚所在端口
    u16 rs_pin;             // RS 引脚
    void* e_port;           // E 引脚所在端口
    u16 e_pin;              // E 引脚
    
    // 数据引脚。如果是4线，使用 data_pins[0-3]；如果是8线，使用 data_pins[0-7]
    void* data_ports[8];    
    u16 data_pins[8];
} lcd1602_t;

/**
 * @brief LCD1602 初始化
 * 
 * @param self 驱动对象
 */
void lcd1602_init(lcd1602_t* self);

/**
 * @brief 清屏
 * 
 * @param self 驱动对象
 */
void lcd1602_clear(lcd1602_t* self);

/**
 * @brief 设置光标位置
 * 
 * @param self 驱动对象
 * @param x 列 (0-15)
 * @param y 行 (0-1)
 */
void lcd1602_set_cursor(lcd1602_t* self, u8 x, u8 y);

/**
 * @brief 打印字符串
 * 
 * @param self 驱动对象
 * @param str 字符串
 */
void lcd1602_print(lcd1602_t* self, const char* str);

/**
 * @brief 打印一个字符
 * 
 * @param self 驱动对象
 * @param ch 字符
 */
void lcd1602_print_char(lcd1602_t* self, char ch);

/**
 * @brief 发送命令
 * 
 * @param self 驱动对象
 * @param cmd 命令
 */
void lcd1602_write_cmd(lcd1602_t* self, u8 cmd);

/**
 * @brief 发送数据
 * 
 * @param self 驱动对象
 * @param data 数据
 */
void lcd1602_write_data(lcd1602_t* self, u8 data);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_DRIVER_LCD1602_H
