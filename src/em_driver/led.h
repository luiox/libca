/**
 * @file led.h
 * @author canrad (1517807724@qq.com)
 * @brief led驱动，port是否初始化由适配层保证，否则会有空指针风险
 * @version 0.1
 * @date 2026-01-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_LED_H
#define LIBCA_EM_DRIVER_LED_H

#include "../em_base/datatype.h"

// port
typedef struct led_port{
    void (*write_pin)(void* gpio, u16 pin, u8 value);
}led_port_t;

// 绑定port
void led_bind_port(const led_port_t* port);
bool led_port_is_registered(void);

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
