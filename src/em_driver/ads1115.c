#include "ads1115.h"

static const ads1115_port_t* g_port = NULL;

void ads1115_bind_port(const ads1115_port_t* port)
{
    g_port = port;
}

bool ads1115_port_is_registered(void)
{
    return g_port != NULL;
}

/* ADS1115 寄存器地址 */
#define ADS1115_REG_CONVERSE    0x00
#define ADS1115_REG_CONFIG      0x01
#define ADS1115_REG_LO_THRESH   0x02
#define ADS1115_REG_HI_THRESH   0x03

void ads1115_init(ads1115_t* self, void* hi2c, u8 dev_addr)
{
    self->hi2c     = hi2c;
    self->dev_addr = dev_addr;
    self->gain_lsb = 0.000125f;   // 默认 4.096V 量程 (PGA bits = 001)
}

static i32 ads1115_write_reg(ads1115_t* self, u8 reg, u16 value)
{
    if (!g_port || !g_port->i2c_write)
        return ADS1115_ERR_PORT_NOT_REGISTERED;

    u8 data[2];
    data[0] = (u8)(value >> 8);
    data[1] = (u8)(value & 0xFF);

    if (g_port->i2c_write(self->hi2c, self->dev_addr, reg, data, 2) != 0) {
        return ADS1115_ERR_I2C;
    }
    return ADS1115_OK;
}

static i32 ads1115_read_reg(ads1115_t* self, u8 reg, u16* value)
{
    if (!g_port || !g_port->i2c_read)
        return ADS1115_ERR_PORT_NOT_REGISTERED;

    u8 data[2];
    if (g_port->i2c_read(self->hi2c, self->dev_addr, reg, data, 2) != 0) {
        return ADS1115_ERR_I2C;
    }

    *value = (u16)((data[0] << 8) | data[1]);
    return ADS1115_OK;
}

i32 ads1115_config(ads1115_t* self, ads1115_mux mux, ads1115_pga pga, ads1115_mode mode,
                   ads1115_rate rate)
{
    u16 config = 0x8003;   // 默认 OS=1 (开始转换), COMP_QUE=3 (关闭比较器)
    config |= (u16)(mux << 12);
    config |= (u16)(pga << 9);
    config |= (u16)(mode << 8);
    config |= (u16)(rate << 5);

    i32 ret = ads1115_write_reg(self, ADS1115_REG_CONFIG, config);
    if (ret != ADS1115_OK)
        return ret;

    // 更新 LSB 增益系数
    switch (pga) {
    case ADS1115_PGA_6144: self->gain_lsb = 6.144f / 32768.0f; break;
    case ADS1115_PGA_4096: self->gain_lsb = 4.096f / 32768.0f; break;
    case ADS1115_PGA_2048: self->gain_lsb = 2.048f / 32768.0f; break;
    case ADS1115_PGA_1024: self->gain_lsb = 1.024f / 32768.0f; break;
    case ADS1115_PGA_0512: self->gain_lsb = 0.512f / 32768.0f; break;
    case ADS1115_PGA_0256: self->gain_lsb = 0.256f / 32768.0f; break;
    default: self->gain_lsb = 2.048f / 32768.0f; break;
    }

    return ADS1115_OK;
}

i32 ads1115_read_raw(ads1115_t* self, i16* raw_val)
{
    u16 val;
    i32 ret = ads1115_read_reg(self, ADS1115_REG_CONVERSE, &val);
    if (ret == ADS1115_OK) {
        *raw_val = (i16)val;
    }
    return ret;
}

i32 ads1115_read_voltage(ads1115_t* self, f32* voltage)
{
    i16 raw;
    i32 ret = ads1115_read_raw(self, &raw);
    if (ret == ADS1115_OK) {
        *voltage = (f32)raw * self->gain_lsb;
    }
    return ret;
}
