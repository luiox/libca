#include "jy61p.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK i32 port_jy61p_uart_send(void* huart, const u8* buf, usize len)
{
    (void)huart;
    (void)buf;
    (void)len;
    return 0;
}

CA_WEAK void port_jy61p_delay_ms(u32 ms)
{
    (void)ms;
}
