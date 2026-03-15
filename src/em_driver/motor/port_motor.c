#include "motor.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK void port_motor_pwm_set_duty(void* htim, u16 channel, u8 duty)
{
    (void)htim;
    (void)channel;
    (void)duty;
}

CA_WEAK void port_motor_pwm_start(void* htim, u16 channel)
{
    (void)htim;
    (void)channel;
}

CA_WEAK void port_motor_pwm_stop(void* htim, u16 channel)
{
    (void)htim;
    (void)channel;
}
