#include "as5600.h"
#include "em_base/debug.h"

// AS5600 I2C 7位地址
#define AS5600_ADDR 0x36

// 私有寄存器定义
#define AS5600_REG_STATUS 0x0B
#define AS5600_REG_RAW_HI 0x0C
#define AS5600_REG_RAW_LO 0x0D
#define AS5600_REG_AGC 0x1A

static const as5600_port_t* g_as5600_port = NULL;

void as5600_bind_port(const as5600_port_t* port)
{
    param_check(port != NULL);
    g_as5600_port = port;
}

bool as5600_port_is_registered(void)
{
    return g_as5600_port != NULL;
}

void as5600_init(as5600_t* self, void* hi2c)
{
    param_check(self != NULL);
    param_check(hi2c != NULL);

    self->hi2c = hi2c;
}

u16 as5600_read_raw_angle(as5600_t* self)
{
    param_check(self != NULL);
    param_check(as5600_port_is_registered());

    u8 buf[2] = {0};
    // AS5600支持连续读取，高位寄存器地址为0x0C，低位为0x0D
    g_as5600_port->i2c_read(self->hi2c, AS5600_ADDR, AS5600_REG_RAW_HI, buf, 2);

    u16 val = ((u16)buf[0] << 8) | (u16)buf[1];
    return val & 0x0FFF;
}

f32 as5600_read_angle(as5600_t* self)
{
    param_check(self != NULL);
    
    u16 raw = as5600_read_raw_angle(self);
    return as5600_raw_to_degree(raw);
}

f32 as5600_raw_to_degree(u16 angle)
{
    // 角度（degree） = raw / 4096.0 * 360.0。raw 为 0…4095（12-bit），即每 LSB ≈ 360/4096 =
    // 0.087890625°。
    return ((f32)angle) * 360.0f / 4096.0f;
}

u8 as5600_get_status(as5600_t* self)
{
    param_check(self != NULL);

    u8 status = 0;
    g_as5600_port->i2c_read(self->hi2c, AS5600_ADDR, AS5600_REG_STATUS, &status, 1);
    return status;
}

u8 as5600_get_agc(as5600_t* self)
{
    param_check(self != NULL);

    u8 agc = 0;
    g_as5600_port->i2c_read(self->hi2c, AS5600_ADDR, AS5600_REG_AGC, &agc, 1);
    return agc;
}
