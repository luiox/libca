#include "jy61p.h"
#include <em_base/compiler_compat.h>

CA_WEAK i32 port_jy61p_uart_send(void* huart, const u8* buf, usize len)
{
    unused_param(huart);
    unused_param(buf);
    unused_param(len);
    return 0;
}

CA_WEAK void port_jy61p_delay_ms(u32 ms)
{
    unused_param(ms);
}
