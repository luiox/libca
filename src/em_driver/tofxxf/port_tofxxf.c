#include "tofxxf.h"
#include <em_base/compiler_compat.h>

CA_WEAK i32 port_tofxxf_uart_send(void* huart, const u8* data, usize len)
{
    unused_param(huart);
    unused_param(data);
    unused_param(len);
    return 0;
}

CA_WEAK i32 port_tofxxf_uart_recv(void* huart, u8* buf, usize len, u32 timeout_ms)
{
    unused_param(huart);
    unused_param(buf);
    unused_param(len);
    unused_param(timeout_ms);
    return 0;
}
