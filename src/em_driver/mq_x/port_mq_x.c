#include "mq_x.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK u16 port_mqx_read_adc(void* adc, u8 channel)
{
    (void)adc;
    (void)channel;
    return 0;
}

CA_WEAK u8 port_mqx_read_pin(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
    return 0;
}
