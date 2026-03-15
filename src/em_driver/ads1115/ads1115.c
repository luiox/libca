#include "ads1115.h"

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_ADS1115_PORT_MODE == LIBCA_ADS1115_PORT_MODE_EXTERN)

#define ADS1115_I2C_WRITE(hi2c, dev_addr, reg_addr, data, size) \
    port_ads1115_i2c_write((hi2c), (dev_addr), (reg_addr), (data), (size))
#define ADS1115_I2C_READ(hi2c, dev_addr, reg_addr, data, size) \
    port_ads1115_i2c_read((hi2c), (dev_addr), (reg_addr), (data), (size))
#define ADS1115_DELAY_MS(ms) port_ads1115_delay_ms((ms))

#elif (LIBCA_ADS1115_PORT_MODE == LIBCA_ADS1115_PORT_MODE_DYNAMIC)

static const ads1115_port_t* g_ads1115_port = NULL;

#define ADS1115_I2C_WRITE(hi2c, dev_addr, reg_addr, data, size) \
    g_ads1115_port->i2c_write((hi2c), (dev_addr), (reg_addr), (data), (size))
#define ADS1115_I2C_READ(hi2c, dev_addr, reg_addr, data, size) \
    g_ads1115_port->i2c_read((hi2c), (dev_addr), (reg_addr), (data), (size))
#define ADS1115_DELAY_MS(ms) g_ads1115_port->delay_ms((ms))

#else
#error "Invalid ADS1115 port mode"
#endif

#if (LIBCA_ADS1115_PORT_MODE == LIBCA_ADS1115_PORT_MODE_DYNAMIC)

void ads1115_bind_port(const ads1115_port_t* port)
{
    g_ads1115_port = port;
}

bool ads1115_port_is_registered(void)
{
    return g_ads1115_port != NULL;
}

#endif

////////////////////////////////////////////////////////////////////////////////

/* ADS1115 寄存器地址 */
#define ADS1115_REG_CONVERSE    0x00
#define ADS1115_REG_CONFIG      0x01
#define ADS1115_REG_LO_THRESH   0x02
#define ADS1115_REG_HI_THRESH   0x03

void ads1115_init(ads1115_t* self, void* hi2c, u8 dev_addr)
{
    self->hi2c     = hi2c;
    self->dev_addr = dev_addr;
    self->gain_lsb = 2.048f / 32768.0f; // 默认 ±2.048V
    self->mux      = ADS1115_MUX_DIFF_0_1;
    self->pga      = ADS1115_PGA_2048;
    self->mode     = ADS1115_MODE_SINGLE;
    self->rate     = ADS1115_RATE_128;
}

static i32 ads1115_write_reg(ads1115_t* self, u8 reg, u16 value)
{
    u8 data[2];
    data[0] = (u8)(value >> 8);
    data[1] = (u8)(value & 0xFF);

    if (ADS1115_I2C_WRITE(self->hi2c, self->dev_addr, reg, data, 2) != 0) {
        return ADS1115_ERR_I2C;
    }
    return ADS1115_OK;
}

static i32 ads1115_read_reg(ads1115_t* self, u8 reg, u16* value)
{
    u8 data[2];
    if (ADS1115_I2C_READ(self->hi2c, self->dev_addr, reg, data, 2) != 0) {
        return ADS1115_ERR_I2C;
    }

    *value = (u16)((data[0] << 8) | data[1]);
    return ADS1115_OK;
}

i32 ads1115_config(ads1115_t* self, ads1115_mux mux, ads1115_pga pga, ads1115_mode mode,
                   ads1115_rate rate)
{
    self->mux  = mux;
    self->pga  = pga;
    self->mode = mode;
    self->rate = rate;

    u16 config = 0x0003;   // 默认 COMP_QUE=3 (关闭比较器)
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
    i32 ret;

    if (self->mode == ADS1115_MODE_SINGLE) {
        u16 config;
        ret = ads1115_read_reg(self, ADS1115_REG_CONFIG, &config);
        if (ret != ADS1115_OK)
            return ret;

        // 设置 OS 位以开始单次转换
        config |= (1 << 15);
        ret = ads1115_write_reg(self, ADS1115_REG_CONFIG, config);
        if (ret != ADS1115_OK)
            return ret;

        // 通过轮询 OS 位来等待转换完成 (当 OS 位为 1 时表示完成)
        // 根据采样率计算一个合理的超时时间
        const u16 conversion_time_ms[] = { 126, 63, 32, 16, 8, 4, 3, 2 };   // 对应各速率的转换时间(ms)
        u16       timeout_ms           = conversion_time_ms[self->rate];
        u16       poll_count           = 0;
        do {
            ADS1115_DELAY_MS(1);
            ret = ads1115_read_reg(self, ADS1115_REG_CONFIG, &config);
            if (ret != ADS1115_OK)
                return ret;
            poll_count++;
        } while (!((config >> 15) & 1) && poll_count < timeout_ms);

        if (poll_count >= timeout_ms) {
            return ADS1115_ERR_TIMEOUT;
        }
    }

    u16 val;
    ret = ads1115_read_reg(self, ADS1115_REG_CONVERSE, &val);
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
