#include "ir_track.h"
#include <em_base/compiler_compat.h>

CA_WEAK u8 port_ir_track_read_pin(void* gpio, u16 pin)
{
    unused_param(gpio);
    unused_param(pin);
    return 0;
}
