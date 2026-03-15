#include "hc_sr04.h"
#include <em_base/compiler_compat.h>

CA_WEAK void port_hc_sr04_write_pin(void* gpio, u16 pin, u8 value)
{
    unused_param(gpio);
    unused_param(pin);
    unused_param(value);
}

CA_WEAK u8 port_hc_sr04_read_pin(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
    return 0;
}

CA_WEAK void port_hc_sr04_delay_us(u32 us)
{
    unused_param(us);
}

CA_WEAK void port_hc_sr04_tim_set_counter(void* tim, u32 val)
{
    unused_param(tim);
    unused_param(val);
}

CA_WEAK void port_hc_sr04_tim_start(void* tim)
{
    unused_param(tim);
}

CA_WEAK void port_hc_sr04_tim_stop(void* tim)
{
    unused_param(tim);
}

CA_WEAK u32 port_hc_sr04_tim_get_counter(void* tim)
{
    unused_param(tim);
    return 0;
}

CA_WEAK void port_hc_sr04_mutex_pend(void)
{
}

CA_WEAK void port_hc_sr04_mutex_post(void)
{
}
