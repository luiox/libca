#include "as5600.h"
#include "../em_base/debug.h"

// AS5600 I2C 7位地址
#define AS5600_ADDR             0x36

// 私有寄存器定义
#define AS5600_RAW_HI_REG       0x0C
#define AS5600_RAW_LO_REG       0x0D

static const as5600_port_t* g_as5600_port = NULL;

void as5600_bind_port(const as5600_port_t* port)
{
    g_as5600_port = port;
}

bool as5600_port_is_registered(void)
{
    return g_as5600_port != NULL;
}

void as5600_init(as5600_t* self, void* hi2c)
{
    self->hi2c = hi2c;
}

u16 as5600_read_raw_angle(as5600_t* self)
{
    if (!as5600_port_is_registered()) {
        debug_print("[as5600] error: port not registered\n");
        return 0;
    }

    u8 buf[2] = {0};
    // AS5600支持连续读取，高位寄存器地址为0x0C，低位为0x0D
    g_as5600_port->i2c_read(self->hi2c, AS5600_ADDR, AS5600_RAW_HI_REG, buf, 2);
    
    u16 val = ((u16)buf[0] << 8) | (u16)buf[1];
    return val & 0x0FFF;
}

f32 as5600_raw_to_degree(u16 angle)
{
    return ((f32)angle) * 360.0f / 4096.0f;
}
