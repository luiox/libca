#include "bmc050.h"
#include "em_base/debug.h"

// Internal register and bit definitions (hidden from public header)
#define BMC050_REG_ACC_WHO_AM_I                   0x00
#define BMC050_REG_ACC_OUT_XL                     0x02
#define BMC050_REG_ACC_OUT_XH                     0x03
#define BMC050_REG_ACC_OUT_YL                     0x04
#define BMC050_REG_ACC_OUT_YH                     0x05
#define BMC050_REG_ACC_OUT_ZL                     0x06
#define BMC050_REG_ACC_OUT_ZH                     0x07
#define BMC050_REG_ACC_OUT_TEMP                   0x08
#define BMC050_REG_ACC_STATUS_IRQL                0x09
#define BMC050_REG_ACC_STATUS_IRQH                0x0a
#define BMC050_REG_ACC_TS_IRQ                     0x0b
#define BMC050_REG_ACC_FO_IRQ                     0x0c
#define BMC050_REG_ACC_G_RANGE                    0x0f
#define BMC050_REG_ACC_BANDWIDTH                  0x10
#define BMC050_REG_ACC_POWER_MODE                 0x11
#define BMC050_REG_ACC_FILTER                     0x13
#define BMC050_REG_ACC_SOFT_RESET                 0x14
#define BMC050_REG_ACC_IRQ1                       0x16
#define BMC050_REG_ACC_IRQ2                       0x17
#define BMC050_REG_ACC_INT_MAP1                   0x19
#define BMC050_REG_ACC_INT_MAP2                   0x1a
#define BMC050_REG_ACC_INT_MAP3                   0x1b
#define BMC050_REG_ACC_INT_CONFIG                 0x20
#define BMC050_REG_ACC_IRQ_MODE                   0x21
#define BMC050_REG_ACC_SLOPE_SAMPLES              0x27
#define BMC050_REG_ACC_SLOPE_THRESHOLD            0x28
#define BMC050_REG_ACC_IF_CONFIG                  0x34

#define BMC050_REG_MAG_WHO_AM_I                   0x40

#define BMC050_ACC_BW_SHADOW                      0x40
#define BMC050_ACC_SOFT_RESET                     0xb6
#define BMC050_ACC_SUSPEND                        0x80
#define BMC050_ACC_LOWPOWER                       0x40

static bmc050_port_t* g_bmc050_port = NULL;

void bmc050_bind_port(const bmc050_port_t* port)
{
    g_bmc050_port = (bmc050_port_t*)port;
}

bool bmc050_port_is_registered(void)
{
    return g_bmc050_port != NULL;
}

#define I2C_TIMEOUT_DEFAULT 1000

static i32 bmc050_write_reg(bmc050_t* self, u8 reg, u8 value)
{
    if (!g_bmc050_port) {
        debug_print("[bmc050] port not registered");
        return BMC050_ERR_PORT_NOT_REGISTERED;
    }

    i32 ret = g_bmc050_port->i2c_write(self->hi2c, self->dev_addr, reg, 1, &value, 1, I2C_TIMEOUT_DEFAULT);
    if (ret != 0) {
        debug_print("[bmc050] i2c write failed, reg:0x%02x", reg);
        return BMC050_ERR_I2C_FAIL;
    }

    return BMC050_OK;
}

static i32 bmc050_read_reg(bmc050_t* self, u8 reg, u8* value)
{
    if (!g_bmc050_port) {
        debug_print("[bmc050] port not registered");
        return BMC050_ERR_PORT_NOT_REGISTERED;
    }

    i32 ret = g_bmc050_port->i2c_read(self->hi2c, self->dev_addr, reg, 1, value, 1, I2C_TIMEOUT_DEFAULT);
    if (ret != 0) {
        debug_print("[bmc050] i2c read failed, reg:0x%02x", reg);
        return BMC050_ERR_I2C_FAIL;
    }

    return BMC050_OK;
}

void bmc050_init(bmc050_t* self, void* hi2c, u16 dev_addr)
{
    if (!self) return;
    self->hi2c = hi2c;
    self->dev_addr = dev_addr;

    // 默认配置：正常模式、2G、2000Hz、不启用 WDT、INT 引脚推挽高电平
    bmc050_power_normal(self);
    bmc050_set_range(self, BMC050_ACC_FS_2G);
    bmc050_set_bandwidth(self, BMC050_ACC_BW2000);
    bmc050_interface_config(self, BMC050_ACC_IF_WDT_OFF);
    bmc050_int_pin_config(self, BMC050_ACC_INT1_PP | BMC050_ACC_INT2_PP | BMC050_ACC_INT1_HIGH | BMC050_ACC_INT2_HIGH);
}

i32 bmc050_get_device_id(bmc050_t* self, u8* id)
{
    if (!self || !id) return BMC050_ERR_INVALID_PARAM;
    return bmc050_read_reg(self, BMC050_REG_ACC_WHO_AM_I, id);
}

i32 bmc050_read_temperature(bmc050_t* self, int16_t* temp10)
{
    if (!self || !temp10) return BMC050_ERR_INVALID_PARAM;

    u8 raw = 0;
    i32 rc = bmc050_read_reg(self, BMC050_REG_ACC_OUT_TEMP, &raw);
    if (rc != BMC050_OK) return rc;

    // 原实现: (((int8_t)raw * 0.5) + 24.0) * 10
    // 整数计算： raw * 5 + 240
    *temp10 = (int16_t)((int8_t)raw * 5 + 240);
    return BMC050_OK;
}

i32 bmc050_set_range(bmc050_t* self, bmc050_acc_fs range)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    return bmc050_write_reg(self, BMC050_REG_ACC_G_RANGE, (u8)range);
}

i32 bmc050_set_bandwidth(bmc050_t* self, bmc050_acc_bw bw)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;

    if (bw == BMC050_ACC_BW2000) {
        return bmc050_write_reg(self, BMC050_REG_ACC_FILTER, (u8)BMC050_ACC_BW2000);
    }

    u8 val = 0;
    i32 rc = g_bmc050_port->i2c_read(self->hi2c, self->dev_addr, BMC050_REG_ACC_FILTER, 1, &val, 1, I2C_TIMEOUT_DEFAULT);
    if (rc != 0) {
        debug_print("[bmc050] i2c read failed (filter)");
        return BMC050_ERR_I2C_FAIL;
    }

    val &= ~BMC050_ACC_BW2000;
    rc = bmc050_write_reg(self, BMC050_REG_ACC_FILTER, val);
    if (rc != BMC050_OK) return rc;

    return bmc050_write_reg(self, BMC050_REG_ACC_BANDWIDTH, (u8)bw);
}

i32 bmc050_soft_reset(bmc050_t* self)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    return bmc050_write_reg(self, BMC050_REG_ACC_SOFT_RESET, (u8)BMC050_ACC_SOFT_RESET);
}

i32 bmc050_power_normal(bmc050_t* self)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    return bmc050_write_reg(self, BMC050_REG_ACC_POWER_MODE, 0);
}

i32 bmc050_suspend(bmc050_t* self)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    return bmc050_write_reg(self, BMC050_REG_ACC_POWER_MODE, (u8)BMC050_ACC_SUSPEND);
}

i32 bmc050_low_power(bmc050_t* self, bmc050_acc_sleep sleep_duration)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    u8 data = (u8)((BMC050_ACC_LOWPOWER | sleep_duration) & 0x5e);
    return bmc050_write_reg(self, BMC050_REG_ACC_POWER_MODE, data);
}

static i32 bmc050_read_axis(bmc050_t* self, u8 reg_lsb, int16_t* out)
{
    if (!self || !out) return BMC050_ERR_INVALID_PARAM;
    u8 buf[2] = {0};
    i32 rc = g_bmc050_port->i2c_read(self->hi2c, self->dev_addr, reg_lsb, 1, buf, 2, I2C_TIMEOUT_DEFAULT);
    if (rc != 0) {
        debug_print("[bmc050] i2c read failed (axis)");
        return BMC050_ERR_I2C_FAIL;
    }

    *out = (int16_t)((int16_t)(buf[1] << 8) | (int16_t)(buf[0] >> 6));
    return BMC050_OK;
}

i32 bmc050_get_x(bmc050_t* self, int16_t* x)
{
    return bmc050_read_axis(self, BMC050_REG_ACC_OUT_XL, x);
}

i32 bmc050_get_y(bmc050_t* self, int16_t* y)
{
    return bmc050_read_axis(self, BMC050_REG_ACC_OUT_YL, y);
}

i32 bmc050_get_z(bmc050_t* self, int16_t* z)
{
    return bmc050_read_axis(self, BMC050_REG_ACC_OUT_ZL, z);
}

i32 bmc050_get_xyz(bmc050_t* self, int16_t* x, int16_t* y, int16_t* z)
{
    if (!self || !x || !y || !z) return BMC050_ERR_INVALID_PARAM;

    u8 buf[6] = {0};
    i32 rc = g_bmc050_port->i2c_read(self->hi2c, self->dev_addr, BMC050_REG_ACC_OUT_XL, 1, buf, 6, I2C_TIMEOUT_DEFAULT);
    if (rc != 0) {
        debug_print("[bmc050] i2c read failed (xyz)");
        return BMC050_ERR_I2C_FAIL;
    }

    *x = (int16_t)((int16_t)(buf[1] << 8) | (int16_t)(buf[0] >> 6));
    *y = (int16_t)((int16_t)(buf[3] << 8) | (int16_t)(buf[2] >> 6));
    *z = (int16_t)((int16_t)(buf[5] << 8) | (int16_t)(buf[4] >> 6));

    return BMC050_OK;
}

i32 bmc050_set_irq(bmc050_t* self, bmc050_acc_ie irqs)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;

    i32 rc = bmc050_write_reg(self, BMC050_REG_ACC_IRQ1, (u8)( ( (u16)irqs ) >> 8 ));
    if (rc != BMC050_OK) return rc;
    return bmc050_write_reg(self, BMC050_REG_ACC_IRQ2, (u8)( ( (u16)irqs ) & 0xff ));
}

i32 bmc050_get_irq_status(bmc050_t* self, bmc050_acc_irq* status)
{
    if (!self || !status) return BMC050_ERR_INVALID_PARAM;

    u8 buf[2] = {0};
    i32 rc = g_bmc050_port->i2c_read(self->hi2c, self->dev_addr, BMC050_REG_ACC_STATUS_IRQL, 1, buf, 2, I2C_TIMEOUT_DEFAULT);
    if (rc != 0) return BMC050_ERR_I2C_FAIL;

    *status = (bmc050_acc_irq)((buf[0] << 8) | buf[1]);
    return BMC050_OK;
}

i32 bmc050_set_irq_mode(bmc050_t* self, bmc050_acc_im mode)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;

    if (mode == BMC050_ACC_IM_RESET) {
        u8 val = 0;
        i32 rc = g_bmc050_port->i2c_read(self->hi2c, self->dev_addr, BMC050_REG_ACC_IRQ_MODE, 1, &val, 1, I2C_TIMEOUT_DEFAULT);
        if (rc != 0) return BMC050_ERR_I2C_FAIL;
        val |= BMC050_ACC_IM_RESET;
        return bmc050_write_reg(self, BMC050_REG_ACC_IRQ_MODE, val);
    }

    return bmc050_write_reg(self, BMC050_REG_ACC_IRQ_MODE, (u8)mode);
}

i32 bmc050_config_slope_irq(bmc050_t* self, u8 nSamples, u8 threshold)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    i32 rc = bmc050_write_reg(self, BMC050_REG_ACC_SLOPE_SAMPLES, nSamples & 0x03);
    if (rc != BMC050_OK) return rc;
    return bmc050_write_reg(self, BMC050_REG_ACC_SLOPE_THRESHOLD, threshold);
}

i32 bmc050_get_ts_irq(bmc050_t* self, u8* status)
{
    if (!self || !status) return BMC050_ERR_INVALID_PARAM;
    return bmc050_read_reg(self, BMC050_REG_ACC_TS_IRQ, status);
}

i32 bmc050_interface_config(bmc050_t* self, bmc050_acc_if mode)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    return bmc050_write_reg(self, BMC050_REG_ACC_IF_CONFIG, (u8)(mode & 0x06));
}

i32 bmc050_int_pin_config(bmc050_t* self, bmc050_acc_intconfig mode)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    return bmc050_write_reg(self, BMC050_REG_ACC_INT_CONFIG, (u8)(mode & 0x0f));
}

i32 bmc050_int_pin_map(bmc050_t* self, bmc050_acc_intmap map)
{
    if (!self) return BMC050_ERR_INVALID_PARAM;
    i32 rc = bmc050_write_reg(self, BMC050_REG_ACC_INT_MAP1, (u8)(map >> 16));
    if (rc != BMC050_OK) return rc;
    rc = bmc050_write_reg(self, BMC050_REG_ACC_INT_MAP2, (u8)(map >> 8));
    if (rc != BMC050_OK) return rc;
    return bmc050_write_reg(self, BMC050_REG_ACC_INT_MAP3, (u8)(map & 0xff));
}

i32 bmc050_mag_get_device_id(bmc050_t* self, u8* id)
{
    if (!self || !id) return BMC050_ERR_INVALID_PARAM;
    return bmc050_read_reg(self, BMC050_REG_MAG_WHO_AM_I, id);
}
