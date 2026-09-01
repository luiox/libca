#include <em_driver/led/led.h>

int main(void)
{
    led_t led = {0};
    led_init(&led, 0, 13, 1);
    led_on(&led);
    led_off(&led);
    led_toggle(&led);

    return 0;
}
