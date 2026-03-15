#include "dht11.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK void port_dht11_write_pin(void* gpio, u16 pin, u8 value)
{
    (void)gpio;
    (void)pin;
    (void)value;
}

CA_WEAK u8 port_dht11_read_pin(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
    return 0;
}

CA_WEAK void port_dht11_set_output_mode(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
}

CA_WEAK void port_dht11_set_input_mode(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
}

CA_WEAK void port_dht11_delay_us(u32 us)
{
    (void)us;
}

CA_WEAK void port_dht11_delay_ms(u32 ms)
{
    (void)ms;
}

CA_WEAK u32 port_dht11_get_tick_us(void)
{
    return 0;
}
