#include "illume.h"
#include <em_base/compiler_compat.h>

CA_WEAK u16 port_illume_read_adc(void* adc, u8 channel)
{
    unused_param(adc);
    unused_param(channel);
    return 0;
}

CA_WEAK u8 port_illume_read_pin(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
    return 0;
}
