#include "led.h"

static led_port_t* g_led_port = NULL;

void led_bind_port(const led_port_t* port)
{
    g_led_port = (led_port_t*)port;
}

bool led_port_is_registered(void)
{
    return g_led_port != NULL;
}

// 初始化led
void led_init(led_t* led, void* gpio, u16 pin, u8 valid)
{
    led->gpio  = gpio;
    led->pin   = pin;
    led->valid = valid;
    led->state = led_state_unknown;
}

// 开灯
void led_on(led_t* led)
{
    if (led->valid) {
        g_led_port->write_pin(led->gpio, led->pin, 1);
    }
    else {
        g_led_port->write_pin(led->gpio, led->pin, 0);
    }
    led->state = led_state_on;
}

// 关灯
void led_off(led_t* led)
{
    if (led->valid) {
        g_led_port->write_pin(led->gpio, led->pin, 0);
    }
    else {
        g_led_port->write_pin(led->gpio, led->pin, 1);
    }
    led->state = led_state_off;
}

// 切换灯的状态
void led_toggle(led_t* led)
{
    // 判断当前状态
    if (led->state == led_state_on) {
        led_off(led);
    }
    else if (led->state == led_state_off) {
        led_on(led);
    }
}
