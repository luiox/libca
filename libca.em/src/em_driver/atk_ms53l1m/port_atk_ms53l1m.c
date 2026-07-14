#include "atk_ms53l1m.h"
#include <em_base/compiler_compat.h>

/// @brief 外部隐式注入的弱符号接口实现
///  

CA_WEAK void port_atk_ms53l1m_uart_init(u32 baudrate)
{
    unused_param(baudrate);
}

CA_WEAK void port_atk_ms53l1m_uart_send(u8* buf, u16 len)
{
    unused_param(buf);
    unused_param(len);
}

CA_WEAK u8* port_atk_ms53l1m_uart_rx_get_frame(void)
{
    return NULL;
}

CA_WEAK u16 port_atk_ms53l1m_uart_rx_get_frame_len(void)
{
    return 0;
}

CA_WEAK void port_atk_ms53l1m_uart_rx_restart(void)
{
}

CA_WEAK void port_atk_ms53l1m_delay_ms(u32 ms)
{
    unused_param(ms);
}