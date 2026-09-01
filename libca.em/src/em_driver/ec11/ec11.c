#include "ec11.h"
#include <em_base/debug.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_EC11_PORT_MODE == LIBCA_EC11_PORT_MODE_EXTERN)
#    define EC11_READ_PIN(gpio, pin) port_ec11_read_pin((gpio), (pin))
#elif (LIBCA_EC11_PORT_MODE == LIBCA_EC11_PORT_MODE_DYNAMIC)
static const ec11_port_t* g_ec11_port = NULL;
#    define EC11_READ_PIN(gpio, pin) g_ec11_port->read_pin((gpio), (pin))
#else
#    error "Invalid EC11 port mode"
#endif

#if (LIBCA_EC11_PORT_MODE == LIBCA_EC11_PORT_MODE_DYNAMIC)
void ec11_bind_port(const ec11_port_t* port)
{
    g_ec11_port = port;
}
bool ec11_port_is_registered(void)
{
    return g_ec11_port != NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////

void ec11_init(ec11_t* self, void* clk_gpio, u16 clk_pin, void* dt_gpio, u16 dt_pin, void* sw_gpio,
               u16 sw_pin, u8 sw_when_down_state)
{
    self->clk_gpio           = clk_gpio;
    self->clk_pin            = clk_pin;
    self->dt_gpio            = dt_gpio;
    self->dt_pin             = dt_pin;
    self->sw_gpio            = sw_gpio;
    self->sw_pin             = sw_pin;
    self->sw_when_down_state = sw_when_down_state;

    self->rotation_count = 0;
    self->last_item      = EC11_ROTATION_NONE;

    self->last_clk_state = EC11_READ_PIN(self->clk_gpio, self->clk_pin);
    self->last_dt_state  = EC11_READ_PIN(self->dt_gpio, self->dt_pin);
    self->last_sw_state  = EC11_READ_PIN(self->sw_gpio, self->sw_pin);
}

ec11_rotation ec11_scan(ec11_t* self)
{
    u8 clk_state        = EC11_READ_PIN(self->clk_gpio, self->clk_pin);
    u8 dt_state         = EC11_READ_PIN(self->dt_gpio, self->dt_pin);
    self->last_sw_state = EC11_READ_PIN(self->sw_gpio, self->sw_pin);

    ec11_rotation result = EC11_ROTATION_NONE;

    // 检测 CLK 跳变
    // 通过分析，当 clk_state != dt_state 时为正转，当 clk_state == dt_state 时为反转
    // 原始代码跟在后面的#if 0 ... #endif内
    if (clk_state != self->last_clk_state) {
        // 当CLK和DT电平不同时，为正转
        if (clk_state != dt_state) {
            result = EC11_ROTATION_RIGHT;
            self->rotation_count++;
        }
        else {   // 当CLK和DT电平相同时，为反转
            result = EC11_ROTATION_LEFT;
            self->rotation_count--;
        }
        self->last_item = result;
    }
#if 0
    if (clk_state != self->last_clk_state) {
        if (clk_state == 1) {   // CLK 上升沿
            if (dt_state == 0) {
                result = EC11_ROTATION_RIGHT;   // 正转
                self->rotation_count++;
            }
            else {
                result = EC11_ROTATION_LEFT;   // 反转
                self->rotation_count--;
            }
        }
        else {   // CLK 下降沿
            if (dt_state == 1) {
                result = EC11_ROTATION_RIGHT;   // 正转
                self->rotation_count++;
            }
            else {
                result = EC11_ROTATION_LEFT;   // 反转
                self->rotation_count--;
            }
        }
        self->last_item = result;
    }
#endif

    self->last_clk_state = clk_state;
    self->last_dt_state  = dt_state;

    return result;
}

i32 ec11_get_count(ec11_t* self)
{
    return self->rotation_count;
}

void ec11_reset_count(ec11_t* self)
{
    self->rotation_count = 0;
}

bool ec11_is_sw_down(ec11_t* self)
{
    // 判断当前引脚电平是否匹配预设的“按下”电平
    return self->last_sw_state == self->sw_when_down_state;
}

ec11_rotation ec11_get_last_rotation(ec11_t* self)
{
    return self->last_item;
}
