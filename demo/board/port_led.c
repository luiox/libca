#include <em_driver/led/led.h>
#include <stdio.h>

void port_led_write_pin(void* gpio, u16 pin, u8 value)
{
    (void)gpio;
    (void)pin;
    (void)value;
    printf("port_led_write_pin called with value: %d\n", value);
}
