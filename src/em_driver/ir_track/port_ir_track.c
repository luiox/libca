#include "ir_track.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK u8 port_ir_track_read_pin(void* gpio, u16 pin)
{
    (void)gpio;
    (void)pin;
    return 0;
}
