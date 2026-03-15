#include "lcd1602.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK void port_lcd1602_write_pin(void* gpio, u16 pin, u8 value)
{
    (void)gpio;
    (void)pin;
    (void)value;
}

CA_WEAK void port_lcd1602_set_output_mode(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
}

CA_WEAK void port_lcd1602_delay_us(u32 us)
{
    (void)us;
}

CA_WEAK void port_lcd1602_delay_ms(u32 ms)
{
    (void)ms;
}
