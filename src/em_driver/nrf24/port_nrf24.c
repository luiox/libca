#include "nrf24.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK void port_nrf24_write_pin(void* gpio, u16 pin, u8 value)
{
    (void)gpio;
    (void)pin;
    (void)value;
}

CA_WEAK u8 port_nrf24_read_pin(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
    return 0;
}

CA_WEAK void port_nrf24_set_output_mode(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
}

CA_WEAK void port_nrf24_set_input_mode(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
}

CA_WEAK void port_nrf24_delay_us(u32 us)
{
    (void)us;
}

CA_WEAK void port_nrf24_delay_ms(u32 ms)
{
    (void)ms;
}

CA_WEAK u8 port_nrf24_spi_send_recv(void* hspi, u8 data)
{
    (void)hspi;
    (void)data;
    return 0;
}
