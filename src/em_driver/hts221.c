#include "hts221.h"
#include <em_base/debug.h>

// 寄存器定义
#define HTS221_CTRL_REG1	0x20
#define HTS221_CTRL_REG2	0x21
#define HTS221_CTRL_REG3	0x22

#define HTS221_STATUS_REG	0x27

#define HTS221_HUMIDITY_OUT_L	0x28
#define HTS221_HUMIDITY_OUT_H	0x29
#define HTS221_TEMP_OUT_L	0x2A
#define HTS221_TEMP_OUT_H	0x2B

static const hts221_port_t* g_hts221_port = NULL;

void hts221_bind_port(const hts221_port_t* port)
{
    g_hts221_port = port;
}

bool hts221_port_is_registered(void)
{
    return g_hts221_port != NULL;
}

// 简化宏（使用 mem_addr_size = 1）
#define HTS221_I2C_WRITE(self, reg, buf, len) (g_hts221_port->i2c_write((self)->hi2c, (self)->dev_addr, (reg), 1, (u8*)(buf), (u16)(len), 0xFFFF))
#define HTS221_I2C_READ(self, reg, buf, len)  (g_hts221_port->i2c_read((self)->hi2c, (self)->dev_addr, (reg), 1, (u8*)(buf), (u16)(len), 0xFFFF))
#define HTS221_DELAY_US(us)                    do{ if (g_hts221_port && g_hts221_port->delay_us) g_hts221_port->delay_us((us)); }while(0)

// 启动一次转换（one-shot）
static i32 hts221_start_once(hts221_t* self)
{
    u8 dat = 0;
    if (HTS221_I2C_READ(self, HTS221_CTRL_REG2, &dat, 1) != 0) {
        debug_print("[hts221] start: read ctrl2 fail\n");
        return HTS221_ERR_I2C_FAIL;
    }
    dat |= 0x01;
    if (HTS221_I2C_WRITE(self, HTS221_CTRL_REG2, &dat, 1) != 0) {
        debug_print("[hts221] start: write ctrl2 fail\n");
        return HTS221_ERR_I2C_FAIL;
    }
    return HTS221_OK;
}

void hts221_init(hts221_t* self, void* hi2c, u16 dev_addr)
{
    self->hi2c = hi2c;
    self->dev_addr = dev_addr;

    if (!g_hts221_port) {
        debug_print("[hts221] init: port not registered\n");
        return;
    }

    u8 cmd;

    // 设置分辨率
    cmd = 0x3F;
    if (HTS221_I2C_WRITE(self, 0x10, &cmd, 1) != 0) {
        debug_print("[hts221] init: write reg 0x10 fail\n");
    }

    // 设置电源、BDU、ODR
    cmd = 0x84; // PD=1
    if (HTS221_I2C_WRITE(self, HTS221_CTRL_REG1, &cmd, 1) != 0) {
        debug_print("[hts221] init: write ctrl1 fail\n");
    }

    // 关闭内部加热、复位模式
    cmd = 0x00;
    if (HTS221_I2C_WRITE(self, HTS221_CTRL_REG2, &cmd, 1) != 0) {
        debug_print("[hts221] init: write ctrl2 fail\n");
    }

    // 关闭数据就绪中断
    cmd = 0x00;
    if (HTS221_I2C_WRITE(self, HTS221_CTRL_REG3, &cmd, 1) != 0) {
        debug_print("[hts221] init: write ctrl3 fail\n");
    }
}

// 读取经过校准并换算后的温度（单位：0.1°C）
i32 hts221_read_temperature(hts221_t* self, int16_t* temperature10)
{
    if (!g_hts221_port) {
        debug_print("[hts221] read_temperature: port not registered\n");
        return HTS221_ERR_PORT_NOT_REGISTERED;
    }
    if (!self || !temperature10) return HTS221_ERR_INVALID_PARAM;

    u8 T0_degC_x8, T1_degC_x8, tmp;
    u16 T0_degC_x8_u16, T1_degC_x8_u16;
    int16_t T0_degC, T1_degC;
    u8 buffer[4];
    int16_t T0_out, T1_out, T_out;
    int32_t tmp32;
    u8 status = 0;
    i32 ret;

    // 1. 读取校准系数
    if (HTS221_I2C_READ(self, 0x32, &T0_degC_x8, 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x33, &T1_degC_x8, 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x35, &tmp, 1) != 0) return HTS221_ERR_I2C_FAIL;

    T0_degC_x8_u16 = (((u16)(tmp & 0x03)) << 8) | ((u16)T0_degC_x8);
    T1_degC_x8_u16 = (((u16)(tmp & 0x0C)) << 6) | ((u16)T1_degC_x8);
    T0_degC = (int16_t)(T0_degC_x8_u16 >> 3);
    T1_degC = (int16_t)(T1_degC_x8_u16 >> 3);

    // 3. 读取 T0_OUT 和 T1_OUT
    if (HTS221_I2C_READ(self, 0x3C, &buffer[0], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x3D, &buffer[1], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x3E, &buffer[2], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x3F, &buffer[3], 1) != 0) return HTS221_ERR_I2C_FAIL;

    T0_out = (int16_t)(((u16)buffer[1] << 8) | buffer[0]);
    T1_out = (int16_t)(((u16)buffer[3] << 8) | buffer[2]);

    // 启动转换并轮询状态（带超时）
    ret = hts221_start_once(self);
    if (ret != HTS221_OK) return ret;

    u32 timeout = 1000; // ms
    while (timeout--) {
        if (HTS221_I2C_READ(self, HTS221_STATUS_REG, &status, 1) != 0) return HTS221_ERR_I2C_FAIL;
        if (status == 0x03) break;
        HTS221_DELAY_US(1000);
    }
    if (timeout == (u32)-1) {
        debug_print("[hts221] read_temperature: status timeout\n");
        return HTS221_ERR_I2C_FAIL;
    }

    if (HTS221_I2C_READ(self, HTS221_TEMP_OUT_L, &buffer[0], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, HTS221_TEMP_OUT_H, &buffer[1], 1) != 0) return HTS221_ERR_I2C_FAIL;

    T_out = (int16_t)(((u16)buffer[1] << 8) | buffer[0]);

    // 线性插值，结果以 0.1°C 为单位
    tmp32 = ((int32_t)(T_out - T0_out)) * ((int32_t)(T1_degC - T0_degC) * 10);
    *temperature10 = (int16_t)(tmp32 / (T1_out - T0_out) + T0_degC * 10);

    return HTS221_OK;
}

// 读取湿度（单位：0.1%RH）
i32 hts221_read_humidity(hts221_t* self, int16_t* humidity10)
{
    if (!g_hts221_port) {
        debug_print("[hts221] read_humidity: port not registered\n");
        return HTS221_ERR_PORT_NOT_REGISTERED;
    }
    if (!self || !humidity10) return HTS221_ERR_INVALID_PARAM;

    u8 buf[2];
    int16_t H0_T0_out, H1_T0_out, H_T_out;
    int16_t H0_rh, H1_rh;
    int32_t tmp;
    u8 status = 0;
    i32 ret;

    // 1. 读取 H0_rH 和 H1_rH
    if (HTS221_I2C_READ(self, 0x30, &buf[0], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x31, &buf[1], 1) != 0) return HTS221_ERR_I2C_FAIL;
    H0_rh = (int16_t)(buf[0] >> 1);
    H1_rh = (int16_t)(buf[1] >> 1);

    // 2. 读取 H0_T0_OUT
    if (HTS221_I2C_READ(self, 0x36, &buf[0], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x37, &buf[1], 1) != 0) return HTS221_ERR_I2C_FAIL;
    H0_T0_out = (int16_t)(((u16)buf[1] << 8) | buf[0]);

    // 3. 读取 H1_T0_OUT
    if (HTS221_I2C_READ(self, 0x3A, &buf[0], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, 0x3B, &buf[1], 1) != 0) return HTS221_ERR_I2C_FAIL;
    H1_T0_out = (int16_t)(((u16)buf[1] << 8) | buf[0]);

    // 启动转换并轮询状态
    ret = hts221_start_once(self);
    if (ret != HTS221_OK) return ret;

    u32 timeout = 1000; // ms
    while (timeout--) {
        if (HTS221_I2C_READ(self, HTS221_STATUS_REG, &status, 1) != 0) return HTS221_ERR_I2C_FAIL;
        if (status == 0x03) break;
        HTS221_DELAY_US(1000);
    }
    if (timeout == (u32)-1) {
        debug_print("[hts221] read_humidity: status timeout\n");
        return HTS221_ERR_I2C_FAIL;
    }

    if (HTS221_I2C_READ(self, HTS221_HUMIDITY_OUT_L, &buf[0], 1) != 0) return HTS221_ERR_I2C_FAIL;
    if (HTS221_I2C_READ(self, HTS221_HUMIDITY_OUT_H, &buf[1], 1) != 0) return HTS221_ERR_I2C_FAIL;
    H_T_out = (int16_t)(((u16)buf[1] << 8) | buf[0]);

    tmp = ((int32_t)(H_T_out - H0_T0_out)) * ((int32_t)(H1_rh - H0_rh) * 10);
    *humidity10 = (int16_t)(tmp / (H1_T0_out - H0_T0_out) + H0_rh * 10);

    if (*humidity10 > 1000) *humidity10 = 1000;

    return HTS221_OK;
}
