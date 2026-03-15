#include "ds18b20.h"
#include <em_base/compiler_compat.h>

CA_WEAK void port_ds18b20_write_pin(void* gpio, u16 pin, u8 value)
{
    unused_param(gpio);
    unused_param(pin);
    unused_param(value);
}

CA_WEAK u8 port_ds18b20_read_pin(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
    return 0;
}

CA_WEAK void port_ds18b20_set_output_mode(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
}

CA_WEAK void port_ds18b20_set_input_mode(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
}

CA_WEAK void port_ds18b20_delay_us(u32 us)
{
    unused_param(us);
}
