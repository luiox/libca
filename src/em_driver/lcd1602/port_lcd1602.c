#include "lcd1602.h"
#include <em_base/compiler_compat.h>

CA_WEAK void port_lcd1602_write_pin(void* gpio, u16 pin, u8 value)
{
    unused_param(gpio);
    unused_param(pin);
    unused_param(value);
}

CA_WEAK void port_lcd1602_set_output_mode(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
}

CA_WEAK void port_lcd1602_delay_us(u32 us)
{
    unused_param(us);
}

CA_WEAK void port_lcd1602_delay_ms(u32 ms)
{
    unused_param(ms);
}
