#include "led.h"
#include <em_base/compiler_compat.h>

CA_WEAK void port_led_write_pin(void* gpio, u16 pin, u8 value)
{
    unused_param(gpio);
    unused_param(pin);
    unused_param(value);
}
