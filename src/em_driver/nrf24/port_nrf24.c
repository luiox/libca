#include "nrf24.h"
#include <em_base/compiler_compat.h>

CA_WEAK void port_nrf24_write_pin(void* gpio, u16 pin, u8 value)
{
    unused_param(gpio);
    unused_param(pin);
    unused_param(value);
}

CA_WEAK u8 port_nrf24_read_pin(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
    return 0;
}

CA_WEAK void port_nrf24_set_output_mode(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
}

CA_WEAK void port_nrf24_set_input_mode(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
}

CA_WEAK void port_nrf24_delay_us(u32 us)
{
    unused_param(us);
}

CA_WEAK void port_nrf24_delay_ms(u32 ms)
{
    unused_param(ms);
}

CA_WEAK u8 port_nrf24_spi_send_recv(void* hspi, u8 data)
{
    unused_param(hspi);
    unused_param(data);
    return 0;
}
