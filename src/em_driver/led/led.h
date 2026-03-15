/**
 * @file led.h
 * @author canrad (1517807724@qq.com)
 * @brief led驱动
 * 驱动分为两种模式，默认为外部模式
 * 1. 隐式注入模式，即外部模式，定义LIBCA_LED_PORT_MODE为LIBCA_LED_PORT_MODE_EXTERN，需要实现port_led_write_pin函数
 * 2. 显式注入模式，即动态模式，定义LIBCA_LED_PORT_MODE为LIBCA_LED_PORT_MODE_DYNAMIC，需要实现led_port_t结构体，并调用led_bind_port函数绑定
 * 注意，对于显式注入模式，port是否初始化由适配层保证，否则会有空指针风险
 * @version 0.2
 * @date 2026-01-09
 * @update 0.2 版本新增隐式注入依赖模式，区分原先的动态模式
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_LED_H
#define LIBCA_EM_DRIVER_LED_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_LED_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_LED_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_LED_PORT_MODE
#define LIBCA_LED_PORT_MODE LIBCA_LED_PORT_MODE_EXTERN
#endif

#if (LIBCA_LED_PORT_MODE == LIBCA_LED_PORT_MODE_EXTERN)

extern void port_led_write_pin(void* gpio, u16 pin, u8 value);

#elif (LIBCA_LED_PORT_MODE == LIBCA_LED_PORT_MODE_DYNAMIC)

// port
typedef struct led_port{
    void (*write_pin)(void* gpio, u16 pin, u8 value);
}led_port_t;

// 绑定port
void led_bind_port(const led_port_t* port);
bool led_port_is_registered(void);

#else

#error "Invalid LED port mode"

#endif

// led灯的状态
typedef enum led_state_enum
{
    led_state_off,
    led_state_on,
    led_state_unknown
} led_state;

typedef struct led
{
    void* gpio;
    u16 pin;
    u8 valid; // 标明这个灯是什么时候亮，如果为1，说明是高电平为有效
    led_state state; // 当前灯的状态
} led_t;

// api

// 初始化led
void led_init(led_t* self, void* gpio, u16 pin, u8 valid);
// 开灯
void led_on(led_t* self);
// 关灯
void led_off(led_t* self);
// 切换灯的状态
void led_toggle(led_t* self);


#endif // !LIBCA_EM_DRIVER_LED_H
