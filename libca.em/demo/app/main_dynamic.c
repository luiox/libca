#include <em_driver/led/led.h>

static void demo_write_pin(void* gpio, u16 pin, u8 value)
{
    (void)gpio;
    (void)pin;
    (void)value;
}

int main(void)
{
    led_port_t port;
    port.write_pin = demo_write_pin;

    led_t led = {0};
    led_bind_port(&port);
    led_init(&led, 0, 7, 1);
    led_on(&led);
    led_off(&led);
    return 0;
}
