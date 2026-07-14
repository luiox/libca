/// @file bmp280.c
/// @author canrad (1517807724@qq.com)
/// @brief BMP280 气压计传感器驱动实现 (Port 绑定风格)
/// @version 0.1
/// @date 2026-01-22
///  

#include "bmp280.h"
#include <em_base/debug.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_BMP280_PORT_MODE == LIBCA_BMP280_PORT_MODE_EXTERN)

#define BMP280_I2C_WRITE(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
    port_bmp280_i2c_write((hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))
#define BMP280_I2C_READ(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
    port_bmp280_i2c_read((hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))
#define BMP280_DELAY_MS(ms) port_bmp280_delay_ms((ms))

#elif (LIBCA_BMP280_PORT_MODE == LIBCA_BMP280_PORT_MODE_DYNAMIC)

static const bmp280_port_t* g_bmp280_port = NULL;

#define BMP280_I2C_WRITE(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
    g_bmp280_port->i2c_write((hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))
#define BMP280_I2C_READ(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
    g_bmp280_port->i2c_read((hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))
#define BMP280_DELAY_MS(ms) g_bmp280_port->delay_ms((ms))

#else
#error "Invalid BMP280 port mode"
#endif

#if (LIBCA_BMP280_PORT_MODE == LIBCA_BMP280_PORT_MODE_DYNAMIC)
void bmp280_bind_port(const bmp280_port_t* port) {
    g_bmp280_port = port;
}

bool bmp280_port_is_registered(void) {
    return g_bmp280_port != NULL;
}
#endif

/* --- 私有辅助函数 --- */

static i32 bmp280_write_reg(bmp280_t* self, u8 reg, u8 val) {
    return BMP280_I2C_WRITE(self->hi2c, self->dev_addr, reg, 1, &val, 1, 100);
}

static i32 bmp280_read_regs(bmp280_t* self, u8 reg, u8* data, u16 len) {
    return BMP280_I2C_READ(self->hi2c, self->dev_addr, reg, 1, data, len, 100);
}

static i32 bmp280_read_reg(bmp280_t* self, u8 reg, u8* val) {
    return bmp280_read_regs(self, reg, val, 1);
}

/// @brief 补偿温度 (Bosch 官方算法)
/// @param ut 原始温度
/// @return 摄氏度 (f32)
///  
static f32 bmp280_compensate_T(bmp280_t* self, i32 ut) {
#if BMP280_CALC_MODE == 1
    i32 v_x1, v_x2;
    v_x1 = ((((ut >> 3) - ((i32)self->calib.dig_T1 << 1))) * ((i32)self->calib.dig_T2)) >> 11;
    v_x2 = (((((ut >> 4) - ((i32)self->calib.dig_T1)) * ((ut >> 4) - ((i32)self->calib.dig_T1))) >> 12) * ((i32)self->calib.dig_T3)) >> 14;
    self->calib.t_fine = v_x1 + v_x2;
    return (f32)((self->calib.t_fine * 5 + 128) >> 8) / 100.0f;
#else
    f32 v_x1, v_x2;
    v_x1 = (((f32)ut) / 16384.0f - ((f32)self->calib.dig_T1) / 1024.0f) * ((f32)self->calib.dig_T2);
    v_x2 = (((f32)ut) / 131072.0f - ((f32)self->calib.dig_T1) / 8192.0f);
    v_x2 = (v_x2 * v_x2) * ((f32)self->calib.dig_T3);
    self->calib.t_fine = (i32)(v_x1 + v_x2);
    return (v_x1 + v_x2) / 5120.0f;
#endif
}

/// @brief 补偿压力 (Bosch 官方算法)
/// @param up 原始压力
/// @return 帕斯卡 (f32)
///  
static f32 bmp280_compensate_P(bmp280_t* self, i32 up) {
#if BMP280_CALC_MODE == 1
    i64 v1, v2, p;
    v1 = (i64)self->calib.t_fine - 128000;
    v2 = v1 * v1 * (i64)self->calib.dig_P6;
    v2 = v2 + ((v1 * (i64)self->calib.dig_P5) << 17);
    v2 = v2 + ((i64)self->calib.dig_P4 << 35);
    v1 = ((v1 * v1 * (i64)self->calib.dig_P3) >> 8) + ((v1 * (i64)self->calib.dig_P2) << 12);
    v1 = (((((i64)1) << 47) + v1)) * ((i64)self->calib.dig_P1) >> 33;
    if (v1 == 0) return 0;
    p = 1048576 - up;
    p = (((p << 31) - v2) * 3125) / v1;
    v1 = (((i64)self->calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    v2 = (((i64)self->calib.dig_P8) * p) >> 19;
    p = ((p + v1 + v2) >> 8) + ((i64)self->calib.dig_P7 << 4);
    return (f32)p / 256.0f;
#else
    f32 v1, v2, p;
    v1 = ((f32)self->calib.t_fine / 2.0f) - 64000.0f;
    v2 = v1 * v1 * ((f32)self->calib.dig_P6) / 32768.0f;
    v2 = v2 + v1 * ((f32)self->calib.dig_P5) * 2.0f;
    v2 = (v2 / 4.0f) + (((f32)self->calib.dig_P4) * 65536.0f);
    v1 = (((f32)self->calib.dig_P3) * v1 * v1 / 524288.0f + ((f32)self->calib.dig_P2) * v1) / 524288.0f;
    v1 = (1.0f + v1 / 32768.0f) * ((f32)self->calib.dig_P1);
    if (v1 == 0.0f) return 0;
    p = 1048576.0f - (f32)up;
    p = (p - (v2 / 4096.0f)) * 6250.0f / v1;
    v1 = ((f32)self->calib.dig_P9) * p * p / 2147483648.0f;
    v2 = p * ((f32)self->calib.dig_P8) / 32768.0f;
    p = p + (v1 + v2 + ((f32)self->calib.dig_P7)) / 16.0f;
    return p;
#endif
}

/* --- 公共接口实现 --- */

i32 bmp280_init(bmp280_t* self, void* hi2c, u16 dev_addr) {
    if (!self) return BMP280_ERR_INVALID_PARAM;

    self->hi2c = hi2c;
    self->dev_addr = dev_addr;

    i32 res = bmp280_check(self);
    if (res != BMP280_OK) return res;

    /* 读取校准参数 (24 字节) */
    u8 buf[24];
    if (bmp280_read_regs(self, BMP280_REG_CALIB, buf, 24) != 0) return BMP280_ERR_I2C_FAIL;

    self->calib.dig_T1 = (u16)((buf[1]  << 8) | buf[0]);
    self->calib.dig_T2 = (i16)((buf[3]  << 8) | buf[2]);
    self->calib.dig_T3 = (i16)((buf[5]  << 8) | buf[4]);
    self->calib.dig_P1 = (u16)((buf[7]  << 8) | buf[6]);
    self->calib.dig_P2 = (i16)((buf[9]  << 8) | buf[8]);
    self->calib.dig_P3 = (i16)((buf[11] << 8) | buf[10]);
    self->calib.dig_P4 = (i16)((buf[13] << 8) | buf[12]);
    self->calib.dig_P5 = (i16)((buf[15] << 8) | buf[14]);
    self->calib.dig_P6 = (i16)((buf[17] << 8) | buf[16]);
    self->calib.dig_P7 = (i16)((buf[19] << 8) | buf[18]);
    self->calib.dig_P8 = (i16)((buf[21] << 8) | buf[20]);
    self->calib.dig_P9 = (i16)((buf[23] << 8) | buf[22]);

    return BMP280_OK;
}

i32 bmp280_check(bmp280_t* self) {
    u8 id = 0;
    if (bmp280_read_reg(self, BMP280_REG_ID, &id) != 0) return BMP280_ERR_I2C_FAIL;
    if (id != BMP280_CHIP_ID) {
        debug_print("[bmp280] error: device not found, id: 0x%02X\n", id);
        return BMP280_ERR_DEVICE_NOT_FOUND;
    }
    return BMP280_OK;
}

i32 bmp280_reset(bmp280_t* self) {
    i32 res = bmp280_write_reg(self, BMP280_REG_RESET, BMP280_SOFT_RESET_VAL);
    if (res == BMP280_OK) BMP280_DELAY_MS(10);
    return res;
}

i32 bmp280_config(bmp280_t* self, bmp280_osrs_t osrs_t, bmp280_osrs_t osrs_p, 
                  bmp280_filter_t filter, bmp280_standby_t standby) {
    u8 ctrl_meas = (osrs_t << 5) | (osrs_p << 2) | BMP280_MODE_SLEEP;
    u8 config = (standby << 5) | (filter << 2);

    if (bmp280_write_reg(self, BMP280_REG_CTRL_MEAS, ctrl_meas) != 0) return BMP280_ERR_I2C_FAIL;
    if (bmp280_write_reg(self, BMP280_REG_CONFIG, config) != 0) return BMP280_ERR_I2C_FAIL;

    return BMP280_OK;
}

i32 bmp280_set_mode(bmp280_t* self, bmp280_mode_t mode) {
    u8 ctrl_meas;
    if (bmp280_read_reg(self, BMP280_REG_CTRL_MEAS, &ctrl_meas) != 0) return BMP280_ERR_I2C_FAIL;
    ctrl_meas = (ctrl_meas & 0xFC) | (mode & 0x03);
    return bmp280_write_reg(self, BMP280_REG_CTRL_MEAS, ctrl_meas);
}

bool bmp280_is_measuring(bmp280_t* self) {
    u8 status;
    if (bmp280_read_reg(self, BMP280_REG_STATUS, &status) != 0) return false;
    return (status & 0x08) != 0;
}

i32 bmp280_read_temp(bmp280_t* self, f32* temperature) {
    u8 buf[3];
    if (bmp280_read_regs(self, BMP280_REG_TEMP_MSB, buf, 3) != 0) return BMP280_ERR_I2C_FAIL;
    i32 ut = (i32)((buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4));
    
    f32 t = bmp280_compensate_T(self, ut);
    if (temperature) *temperature = t;
    return BMP280_OK;
}

i32 bmp280_read_press(bmp280_t* self, f32* pressure) {
    /* 压力计算依赖 t_fine，必须先读温度 */
    f32 t;
    i32 res = bmp280_read_temp(self, &t);
    if (res != BMP280_OK) return res;

    u8 buf[3];
    if (bmp280_read_regs(self, BMP280_REG_PRESS_MSB, buf, 3) != 0) return BMP280_ERR_I2C_FAIL;
    i32 up = (i32)((buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4));

    f32 p = bmp280_compensate_P(self, up);
    if (pressure) *pressure = p;
    return BMP280_OK;
}

i32 bmp280_read_all(bmp280_t* self, f32* temperature, f32* pressure) {
    u8 buf[6];
    if (bmp280_read_regs(self, BMP280_REG_PRESS_MSB, buf, 6) != 0) return BMP280_ERR_I2C_FAIL;
    
    i32 up = (i32)((buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4));
    i32 ut = (i32)((buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4));

    f32 t = bmp280_compensate_T(self, ut);
    f32 p = bmp280_compensate_P(self, up);

    if (temperature) *temperature = t;
    if (pressure) *pressure = p;

    return BMP280_OK;
}

/* --- 工具函数实现 --- */

f32 bmp280_pa_to_mmhg(f32 pa) {
    return pa * 0.00750061683f;
}

f32 bmp280_pa_to_alt(f32 pa) {
    /* 海拔公式: h = 44330 * [1 - (P/P0)^(1/5.255)] */
    /* P0 = 101325 Pa */
    f32 p_ratio = pa / 101325.0f;
    f32 x = 1.0f - p_ratio;
    return 44330.0f * (0.1902949f * x + 0.0864947f * x * x + 0.0547051f * x * x * x);
}