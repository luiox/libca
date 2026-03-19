#include "led.h"

extern void port_led_write_pin(int value);

void led_on(void)
{
    port_led_write_pin(1);
}
