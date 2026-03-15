#include "motor.h"
#include <em_base/debug.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_MOTOR_PORT_MODE == LIBCA_MOTOR_PORT_MODE_EXTERN)
#define MOTOR_PWM_SET_DUTY(htim, ch, duty) port_motor_pwm_set_duty((htim), (ch), (duty))
#define MOTOR_PWM_START(htim, ch)          port_motor_pwm_start((htim), (ch))
#define MOTOR_PWM_STOP(htim, ch)           port_motor_pwm_stop((htim), (ch))

#elif (LIBCA_MOTOR_PORT_MODE == LIBCA_MOTOR_PORT_MODE_DYNAMIC)

#define MOTOR_PWM_SET_DUTY(htim, ch, duty) g_motor_port->pwm_set_duty((htim), (ch), (duty))
#define MOTOR_PWM_START(htim, ch)          g_motor_port->pwm_start((htim), (ch))
#define MOTOR_PWM_STOP(htim, ch)           g_motor_port->pwm_stop((htim), (ch))

#else
#error "Invalid MOTOR port mode"
#endif

#if (LIBCA_MOTOR_PORT_MODE == LIBCA_MOTOR_PORT_MODE_DYNAMIC)
static const motor_port_t* g_motor_port = NULL;
void motor_bind_port(const motor_port_t* port) 
{ 
    g_motor_port = port; 
}
bool motor_port_is_registered(void) 
{ 
    return g_motor_port != NULL; 
}
#endif

////////////////////////////////////////////////////////////////////////////////

i32 motor_init(motor_t* self, void* htim, u16 channel)
{
    param_check(self != NULL);

    self->htim    = htim;
    self->channel = channel;
    self->duty    = 0;
    self->running = 0;

    return MOTOR_OK;
}

i32 motor_set_duty(motor_t* self, u8 duty)
{
    param_check(self != NULL);

    // 参数检查：占空比范围 0-100
    if (duty > 100) {
        return MOTOR_ERR_INVALID_PARAM;
    }

    self->duty = duty;

    // 如果电机正在运行，立即更新PWM占空比
    if (self->running) {
        MOTOR_PWM_SET_DUTY(self->htim, self->channel, self->duty);
    }

    return MOTOR_OK;
}

u8 motor_get_duty(motor_t* self)
{
    param_check(self != NULL);
    return self->duty;
}

i32 motor_start(motor_t* self)
{
    param_check(self != NULL);

    // 设置PWM占空比并启动
    MOTOR_PWM_SET_DUTY(self->htim, self->channel, self->duty);
    MOTOR_PWM_START(self->htim, self->channel);
    self->running = 1;

    return MOTOR_OK;
}

i32 motor_stop(motor_t* self)
{
    param_check(self != NULL);


    // 停止PWM输出
    MOTOR_PWM_STOP(self->htim, self->channel);
    self->running = 0;

    return MOTOR_OK;
}

bool motor_is_running(motor_t* self)
{
    param_check(self != NULL);
    return self->running != 0;
}
