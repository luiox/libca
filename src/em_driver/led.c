#include "led.h"
#include "../em_base/debug.h"

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
void led_init(led_t* self, void* gpio, u16 pin, u8 valid)
{
    self->gpio  = gpio;
    self->pin   = pin;
    self->valid = valid;
    self->state = led_state_unknown;
}

// 开灯
void led_on(led_t* self)
{
    if (!g_led_port) {
        debug_print("[led] port not registered\n");
        return;
    }

    if (self->valid) {
        g_led_port->write_pin(self->gpio, self->pin, 1);
    }
    else {
        g_led_port->write_pin(self->gpio, self->pin, 0);
    }
    self->state = led_state_on;
}

// 关灯
void led_off(led_t* self)
{
    if (!g_led_port) {
        debug_print("[led] port not registered\n");
        return;
    }

    if (self->valid) {
        g_led_port->write_pin(self->gpio, self->pin, 0);
    }
    else {
        g_led_port->write_pin(self->gpio, self->pin, 1);
    }
    self->state = led_state_off;
}

// 切换灯的状态
void led_toggle(led_t* self)
{
    // 判断当前状态
    if (self->state == led_state_on) {
        led_off(self);
    }
    else if (self->state == led_state_off) {
        led_on(self);
    }
}
