#include "motor.h"
#include "em_base/debug.h"

static const motor_port_t* g_motor_port = NULL;

void motor_bind_port(const motor_port_t* port)
{
    g_motor_port = port;
}

bool motor_port_is_registered(void)
{
    return g_motor_port != NULL;
}

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
    param_check(g_motor_port != NULL);

    // 参数检查：占空比范围 0-100
    if (duty > 100) {
        debug_print("[motor] error: invalid duty value %d, must be 0-100\n", duty);
        return MOTOR_ERR_INVALID_PARAM;
    }

    self->duty = duty;

    // 如果电机正在运行，立即更新PWM占空比
    if (self->running) {
        g_motor_port->pwm_set_duty(self->htim, self->channel, self->duty);
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

    if (!g_motor_port) {
        debug_print("[motor] error: port not registered\n");
        return MOTOR_ERR_PORT_NOT_REGISTERED;
    }

    // 设置PWM占空比并启动
    g_motor_port->pwm_set_duty(self->htim, self->channel, self->duty);
    g_motor_port->pwm_start(self->htim, self->channel);
    self->running = 1;

    return MOTOR_OK;
}

i32 motor_stop(motor_t* self)
{
    param_check(self != NULL);

    if (!g_motor_port) {
        debug_print("[motor] error: port not registered\n");
        return MOTOR_ERR_PORT_NOT_REGISTERED;
    }

    // 停止PWM输出
    g_motor_port->pwm_stop(self->htim, self->channel);
    self->running = 0;

    return MOTOR_OK;
}

bool motor_is_running(motor_t* self)
{
    param_check(self != NULL);
    return self->running != 0;
}
