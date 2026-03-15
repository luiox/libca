#include "ec11.h"
#include <em_base/compiler_compat.h>

CA_WEAK u8 port_ec11_read_pin(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
    return 0;
}
