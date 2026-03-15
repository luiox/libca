#include "hc_sr04.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK void port_hc_sr04_write_pin(void* gpio, u16 pin, u8 value)
{
    (void)gpio;
    (void)pin;
    (void)value;
}

CA_WEAK u8 port_hc_sr04_read_pin(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
    return 0;
}

CA_WEAK void port_hc_sr04_delay_us(u32 us)
{
    (void)us;
}

CA_WEAK void port_hc_sr04_tim_set_counter(void* tim, u32 val)
{
    (void)tim;
    (void)val;
}

CA_WEAK void port_hc_sr04_tim_start(void* tim)
{
    (void)tim;
}

CA_WEAK void port_hc_sr04_tim_stop(void* tim)
{
    (void)tim;
}

CA_WEAK u32 port_hc_sr04_tim_get_counter(void* tim)
{
    (void)tim;
    return 0;
}

CA_WEAK void port_hc_sr04_mutex_pend(void)
{
}

CA_WEAK void port_hc_sr04_mutex_post(void)
{
}
