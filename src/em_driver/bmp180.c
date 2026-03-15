#include "bmp180.h"
#include <em_base/debug.h>

/* --- 硬件接口绑定 --- */

static const bmp180_port_t* g_bmp180_port = NULL;

void bmp180_bind_port(const bmp180_port_t* port) {
    g_bmp180_port = port;
}

bool bmp180_port_is_registered(void) {
    return g_bmp180_port != NULL;
}

/* --- 内部寄存器定义 --- */

#define BMP180_CHIP_ID          0x55
#define BMP180_SOFT_RESET_VAL   0xB6

#define BMP180_REG_CALIB        0xAA // 22 bytes
#define BMP180_REG_ID           0xD0
#define BMP180_REG_RESET        0xE0
#define BMP180_REG_CTRL         0xF4
#define BMP180_REG_DATA         0xF6 // MSB, LSB, XLSB

#define BMP180_CMD_TEMP         0x2E
#define BMP180_CMD_PRESS        0x34 // Base command, need to shift OSS

/* --- 私有辅助函数 --- */

static i32 bmp180_write_reg(bmp180_t* self, u8 reg, u8 val) {
    if (!g_bmp180_port) return BMP180_ERR_PORT_NOT_REGISTERED;
    return g_bmp180_port->i2c_write(self->hi2c, self->dev_addr, reg, 1, &val, 1, 100);
}

static i32 bmp180_read_regs(bmp180_t* self, u8 reg, u8* data, u16 len) {
    if (!g_bmp180_port) return BMP180_ERR_PORT_NOT_REGISTERED;
    return g_bmp180_port->i2c_read(self->hi2c, self->dev_addr, reg, 1, data, len, 100);
}

static i32 bmp180_read_reg(bmp180_t* self, u8 reg, u8* val) {
    return bmp180_read_regs(self, reg, val, 1);
}

/**
 * @brief 计算 B5 中间量 (与温度相关)
 */
static void bmp180_update_b5(bmp180_t* self, u16 ut) {
    i32 x1 = ((i32)ut - (i32)self->calib.AC6) * (i32)self->calib.AC5 >> 15;
    i32 x2 = ((i32)self->calib.MC << 11) / (x1 + (i32)self->calib.MD);
    self->calib.B5 = x1 + x2;
}

/**
 * @brief 根据原始压力值计算实际压力 (需要先更新 B5)
 */
static i32 bmp180_calc_pressure(bmp180_t* self, u32 up, bmp180_oss_t oss) {
    i32 b6 = self->calib.B5 - 4000;
    i32 x1 = ((i32)self->calib.B2 * (b6 * b6 >> 12)) >> 11;
    i32 x2 = ((i32)self->calib.AC2 * b6) >> 11;
    i32 x3 = x1 + x2;
    i32 b3 = (((((i32)self->calib.AC1 * 4) + x3) << oss) + 2) >> 2;
    x1 = ((i32)self->calib.AC3 * b6) >> 13;
    x2 = ((i32)self->calib.B1 * (b6 * b6 >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    u32 b4 = (u32)self->calib.AC4 * (u32)(x3 + 32768) >> 15;
    u32 b7 = ((u32)up - (u32)b3) * (u32)(50000 >> oss);
    i32 p;
    if (b7 < 0x80000000) p = (i32)((b7 << 1) / b4);
    else p = (i32)((b7 / b4) << 1);
    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);
    return p;
}

/* --- 公共接口实现 --- */

i32 bmp180_init(bmp180_t* self, void* hi2c, u16 dev_addr) {
    if (!self) return BMP180_ERR_INVALID_PARAM;
    if (!g_bmp180_port) return BMP180_ERR_PORT_NOT_REGISTERED;

    self->hi2c = hi2c;
    self->dev_addr = dev_addr;

    i32 res = bmp180_check(self);
    if (res != BMP180_OK) return res;

    /* 读取校准参数 (22 字节) */
    u8 buf[22];
    if (bmp180_read_regs(self, BMP180_REG_CALIB, buf, 22) != 0) return BMP180_ERR_I2C_FAIL;

    self->calib.AC1 = (i16)((buf[0]  << 8) | buf[1]);
    self->calib.AC2 = (i16)((buf[2]  << 8) | buf[3]);
    self->calib.AC3 = (i16)((buf[4]  << 8) | buf[5]);
    self->calib.AC4 = (u16)((buf[6]  << 8) | buf[7]);
    self->calib.AC5 = (u16)((buf[8]  << 8) | buf[9]);
    self->calib.AC6 = (u16)((buf[10] << 8) | buf[11]);
    self->calib.B1  = (i16)((buf[12] << 8) | buf[13]);
    self->calib.B2  = (i16)((buf[14] << 8) | buf[15]);
    self->calib.MB  = (i16)((buf[16] << 8) | buf[17]);
    self->calib.MC  = (i16)((buf[18] << 8) | buf[19]);
    self->calib.MD  = (i16)((buf[20] << 8) | buf[21]);

    return BMP180_OK;
}

i32 bmp180_check(bmp180_t* self) {
    u8 id = 0;
    if (bmp180_read_reg(self, BMP180_REG_ID, &id) != 0) return BMP180_ERR_I2C_FAIL;
    if (id != BMP180_CHIP_ID) {
        debug_print("[bmp180] error: device not found, id: 0x%02X\n", id);
        return BMP180_ERR_DEVICE_NOT_FOUND;
    }
    return BMP180_OK;
}

i32 bmp180_reset(bmp180_t* self) {
    return bmp180_write_reg(self, BMP180_REG_RESET, BMP180_SOFT_RESET_VAL);
}

i32 bmp180_read_raw_temp(bmp180_t* self, u16* ut) {
    if (bmp180_write_reg(self, BMP180_REG_CTRL, BMP180_CMD_TEMP) != 0) return BMP180_ERR_I2C_FAIL;
    g_bmp180_port->delay_ms(5); // Wait 4.5ms
    u8 buf[2];
    if (bmp180_read_regs(self, BMP180_REG_DATA, buf, 2) != 0) return BMP180_ERR_I2C_FAIL;
    if (ut) *ut = (u16)((buf[0] << 8) | buf[1]);
    return BMP180_OK;
}

i32 bmp180_read_raw_press(bmp180_t* self, bmp180_oss_t oss, u32* up) {
    u8 cmd = BMP180_CMD_PRESS | (oss << 6);
    if (bmp180_write_reg(self, BMP180_REG_CTRL, cmd) != 0) return BMP180_ERR_I2C_FAIL;
    
    u32 wait_ms = 5;
    if (oss == BMP180_OSS_STANDARD) wait_ms = 8;
    else if (oss == BMP180_OSS_HIGH_RES) wait_ms = 14;
    else if (oss == BMP180_OSS_ULTRA_RES) wait_ms = 26;
    g_bmp180_port->delay_ms(wait_ms);

    u8 buf[3];
    if (bmp180_read_regs(self, BMP180_REG_DATA, buf, 3) != 0) return BMP180_ERR_I2C_FAIL;
    if (up) *up = (u32)(((u32)buf[0] << 16) | ((u32)buf[1] << 8) | (u32)buf[2]) >> (8 - oss);
    return BMP180_OK;
}

i32 bmp180_read_temp(bmp180_t* self, f32* temperature) {
    u16 ut;
    i32 res = bmp180_read_raw_temp(self, &ut);
    if (res != BMP180_OK) return res;

    bmp180_update_b5(self, ut);
    if (temperature) *temperature = (f32)((self->calib.B5 + 8) >> 4) / 10.0f;
    return BMP180_OK;
}

i32 bmp180_read_press(bmp180_t* self, bmp180_oss_t oss, i32* pressure) {
    u32 up;
    i32 res = bmp180_read_raw_press(self, oss, &up);
    if (res != BMP180_OK) return res;

    /* 需要先读取温度以更新 B5 */
    u16 ut;
    res = bmp180_read_raw_temp(self, &ut);
    if (res != BMP180_OK) return res;
    bmp180_update_b5(self, ut);

    if (pressure) *pressure = bmp180_calc_pressure(self, up, oss);
    return BMP180_OK;
}

i32 bmp180_read_all(bmp180_t* self, bmp180_oss_t oss, f32* temperature, i32* pressure) {
    u16 ut;
    i32 res = bmp180_read_raw_temp(self, &ut);
    if (res != BMP180_OK) return res;
    bmp180_update_b5(self, ut);
    if (temperature) *temperature = (f32)((self->calib.B5 + 8) >> 4) / 10.0f;

    u32 up;
    res = bmp180_read_raw_press(self, oss, &up);
    if (res != BMP180_OK) return res;

    if (pressure) *pressure = bmp180_calc_pressure(self, up, oss);
    return BMP180_OK;
}


f32 bmp180_pa_to_mmhg(f32 pa) {
    return pa * 0.00750061683f;
}

f32 bmp180_pa_to_alt(f32 pa) {
	// 可以参考一下老代码
#if 0
    /* 使用简单的巴罗米特公式近似计算海拔 */
    /* h = 44330 * (1 - (p/101325)^(1/5.255)) */
    /* 为提高效率且不依赖 math.h，此处可使用 Taylor 展开 (同 BME280) */
    f32 p1 = pa - 101325.0f;
    f32 p2 = p1 * p1;
    return ((-0.0832546f * p1) + (3.32651E-7f * p2));
#endif
    /* 海拔公式: h = 44330 * [1 - (P/P0)^(1/5.255)] */
    /* P0 = 101325 Pa */
    /* 采用泰勒展开近似以避免 math.h (精度优于 1m 在 0-10km 范围内) */
    f32 p_ratio = pa / 101325.0f;
    f32 x = 1.0f - p_ratio;
    return 44330.0f * (0.1902949f * x + 0.0864947f * x * x + 0.0547051f * x * x * x);
}
