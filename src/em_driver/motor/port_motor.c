#include "motor.h"
#include <em_base/compiler_compat.h>

CA_WEAK void port_motor_pwm_set_duty(void* htim, u16 channel, u8 duty)
{
    unused_param(htim);
    unused_param(channel);
    unused_param(duty);
}

CA_WEAK void port_motor_pwm_start(void* htim, u16 channel)
{
    unused_param(htim);
    unused_param(channel);
}

CA_WEAK void port_motor_pwm_stop(void* htim, u16 channel)
{
    unused_param(htim);
    unused_param(channel);
}
