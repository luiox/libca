#include "led.h"
#include <em_base/debug.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_LED_PORT_MODE == LIBCA_LED_PORT_MODE_EXTERN)
#    define LED_WRITE_PIN(gpio, pin, value) port_led_write_pin((gpio), (pin), (value))

#elif (LIBCA_LED_PORT_MODE == LIBCA_LED_PORT_MODE_DYNAMIC)
static const led_port_t* g_led_port = NULL;
#    define LED_WRITE_PIN(gpio, pin, value) g_led_port->write_pin((gpio), (pin), (value))

#else
#    error "Invalid LED port mode"
#endif

#if (LIBCA_LED_PORT_MODE == LIBCA_LED_PORT_MODE_DYNAMIC)
void led_bind_port(const led_port_t* port)
{
    g_led_port = port;
}
bool led_port_is_registered(void)
{
    return g_led_port != NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// 初始化led
void led_init(led_t* self, void* gpio, u16 pin, u8 valid)
{
    param_check(self != NULL);
    self->gpio  = gpio;
    self->pin   = pin;
    self->valid = valid;
    self->state = led_state_unknown;
}

// 开灯
void led_on(led_t* self)
{
    param_check(self != NULL);
    if (self->valid) {
        LED_WRITE_PIN(self->gpio, self->pin, 1);
    }
    else {
        LED_WRITE_PIN(self->gpio, self->pin, 0);
    }
    self->state = led_state_on;
}

// 关灯
void led_off(led_t* self)
{
    param_check(self != NULL);
    if (self->valid) {
        LED_WRITE_PIN(self->gpio, self->pin, 0);
    }
    else {
        LED_WRITE_PIN(self->gpio, self->pin, 1);
    }
    self->state = led_state_off;
}

// 切换灯的状态
void led_toggle(led_t* self)
{
    param_check(self != NULL);

    // 判断当前状态
    if (self->state == led_state_on) {
        led_off(self);
    }
    else if (self->state == led_state_off) {
        led_on(self);
    }
}
