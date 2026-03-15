#include "tofxxf.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK i32 port_tofxxf_uart_send(void* huart, const u8* data, usize len)
{
    (void)huart;
    (void)data;
    (void)len;
    return 0;
}

CA_WEAK i32 port_tofxxf_uart_recv(void* huart, u8* buf, usize len, u32 timeout_ms)
{
    (void)huart;
    (void)buf;
    (void)len;
    (void)timeout_ms;
    return 0;
}
