/**
 * @file lcd1602.h
 * @author canrad (1517807724@qq.com)
 * 参考文章：https://blog.csdn.net/qq_41701950/article/details/107630078
 * @brief LCD1602 液晶显示屏驱动 (支持4线和8线模式)
 * @version 0.1
 * @date 2026-01-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_LCD1602_H
#define LIBCA_EM_DRIVER_LCD1602_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_LCD1602_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_LCD1602_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_LCD1602_PORT_MODE
#define LIBCA_LCD1602_PORT_MODE LIBCA_LCD1602_PORT_MODE_EXTERN
#endif

// LCD1602 模式枚举
typedef enum lcd1602_mode_enum {
    LCD1602_MODE_4BIT = 0,
    LCD1602_MODE_8BIT = 1,
} lcd1602_mode_t;

#if (LIBCA_LCD1602_PORT_MODE == LIBCA_LCD1602_PORT_MODE_EXTERN)
/** @brief 写引脚电平 @param gpio GPIO端口 @param pin 引脚编号 @param value 电平值 */
extern void port_lcd1602_write_pin(void* gpio, u16 pin, u8 value);
/** @brief 设置引脚为输出模式 @param gpio GPIO端口 @param pin 引脚编号 */
extern void port_lcd1602_set_output_mode(void* gpio, u16 pin);
/** @brief 微秒延时 @param us 延时时间（微秒） */
extern void port_lcd1602_delay_us(u32 us);
/** @brief 毫秒延时 @param ms 延时时间（毫秒） */
extern void port_lcd1602_delay_ms(u32 ms);

#elif (LIBCA_LCD1602_PORT_MODE == LIBCA_LCD1602_PORT_MODE_DYNAMIC)
typedef struct lcd1602_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);  // 写引脚电平
    void (*set_output_mode)(void* gpio, u16 pin);       // 设置输出模式
    void (*delay_us)(u32 us);                           // 微秒延时
    void (*delay_ms)(u32 ms);                           // 毫秒延时
} lcd1602_port_t;
void lcd1602_bind_port(const lcd1602_port_t* port);
bool lcd1602_port_is_registered(void);

#else
#error "Invalid LCD1602 port mode"
#endif

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
    
    // 数据引脚。
    // 8线模式: data_pins[0-7] 对应 LCD 的 D0-D7。
    // 4线模式: data_pins[0-3] 对应 LCD 的 D4-D7。
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

#endif // !LIBCA_EM_DRIVER_LCD1602_H
