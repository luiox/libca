#include "ec11.h"
#include "../em_base/debug.h"

static const ec11_port_t* g_port = NULL;

void ec11_bind_port(const ec11_port_t* port)
{
    g_port = port;
}

bool ec11_port_is_registered(void)
{
    return g_port != NULL;
}

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

    if (g_port) {
        self->last_clk_state = g_port->read_pin(self->clk_gpio, self->clk_pin);
        self->last_dt_state  = g_port->read_pin(self->dt_gpio, self->dt_pin);
        self->last_sw_state  = g_port->read_pin(self->sw_gpio, self->sw_pin);
    }
    else {
        self->last_clk_state = 1;
        self->last_dt_state  = 1;
        self->last_sw_state  = 1;
    }
}

ec11_rotation ec11_scan(ec11_t* self)
{
    if (!g_port) {
        debug_print("[ec11] error: port not registered\n");
        return EC11_ROTATION_NONE;
    }

    u8 clk_state        = g_port->read_pin(self->clk_gpio, self->clk_pin);
    u8 dt_state         = g_port->read_pin(self->dt_gpio, self->dt_pin);
    self->last_sw_state = g_port->read_pin(self->sw_gpio, self->sw_pin);

    ec11_rotation result = EC11_ROTATION_NONE;

    // 检测 CLK 跳变
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
