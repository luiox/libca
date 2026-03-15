#include "bme280.h"
#include <em_base/debug.h>

static const bme280_port_t* g_bme280_port = NULL;

void bme280_bind_port(const bme280_port_t* port)
{
    g_bme280_port = port;
}

bool bme280_port_is_registered(void)
{
    return g_bme280_port != NULL;
}

/* --- 内部寄存器定义 --- */
#define BME280_CHIP_ID 0x60
#define BME280_SOFT_RESET_VAL 0xB6

#define BME280_REG_CALIB00 0x88
#define BME280_REG_CALIB25 0xA1
#define BME280_REG_CALIB26 0xE1
#define BME280_REG_ID 0xD0
#define BME280_REG_RESET 0xE0
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_STATUS 0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG 0xF5
#define BME280_REG_PRESS_MSB 0xF7
#define BME280_REG_TEMP_MSB 0xFA
#define BME280_REG_HUM_MSB 0xFD

#define BME_MMHG_Q0_20 7865 /* 0.00750061683 in Q0.20 format */

/* --- 私有辅助函数 --- */
static i32 bme280_write_reg(bme280_t* self, u8 reg, u8 val)
{
    if (!g_bme280_port)
        return BME280_ERR_PORT_NOT_REGISTERED;
    return g_bme280_port->i2c_write(self->hi2c, self->dev_addr, reg, 1, &val, 1, 100);
}

/* 私有辅助函数: 读多个寄存器 */
static i32 bme280_read_regs(bme280_t* self, u8 reg, u8* data, u16 len)
{
    if (!g_bme280_port)
        return BME280_ERR_PORT_NOT_REGISTERED;
    return g_bme280_port->i2c_read(self->hi2c, self->dev_addr, reg, 1, data, len, 100);
}

/* 私有辅助函数: 读单个寄存器 */
static i32 bme280_read_reg(bme280_t* self, u8 reg, u8* val)
{
    return bme280_read_regs(self, reg, val, 1);
}

/* --- 补偿计算函数实现 --- */

#if BME280_USE_FLOAT

/* 1. 浮点数计算版 (Bosch 原厂推荐) */

static f32 bme280_calc_tf(bme280_t* self, i32 UT)
{
    f32 v_x1, v_x2;
    v_x1 = (((f32)UT) / 16384.0f - ((f32)self->calib.dig_T1) / 1024.0f) * ((f32)self->calib.dig_T2);
    v_x2 = ((f32)UT) / 131072.0f - ((f32)self->calib.dig_T1) / 8192.0f;
    v_x2 = (v_x2 * v_x2) * ((f32)self->calib.dig_T3);
    self->t_fine = (i32)(v_x1 + v_x2);
    return ((v_x1 + v_x2) / 5120.0f);
}

static f32 bme280_calc_pf(bme280_t* self, i32 UP)
{
    f32 v_x1, v_x2, p;
    v_x1 = ((f32)self->t_fine / 2.0f) - 64000.0f;
    v_x2 = v_x1 * v_x1 * ((f32)self->calib.dig_P6) / 32768.0f;
    v_x2 = v_x2 + v_x1 * ((f32)self->calib.dig_P5) * 2.0f;
    v_x2 = (v_x2 / 4.0f) + (((f32)self->calib.dig_P4) * 65536.0f);
    v_x1 =
        (((f32)self->calib.dig_P3) * v_x1 * v_x1 / 524288.0f + ((f32)self->calib.dig_P2) * v_x1) /
        524288.0f;
    v_x1 = (1.0f + v_x1 / 32768.0f) * ((f32)self->calib.dig_P1);
    if (v_x1 == 0.0f)
        return 0.0f;
    p    = 1048576.0f - (f32)UP;
    p    = (p - (v_x2 / 4096.0f)) * 6250.0f / v_x1;
    v_x1 = ((f32)self->calib.dig_P9) * p * p / 2147483648.0f;
    v_x2 = p * ((f32)self->calib.dig_P8) / 32768.0f;
    p += (v_x1 + v_x2 + ((f32)self->calib.dig_P7)) / 16.0f;
    return p;
}

static f32 bme280_calc_hf(bme280_t* self, i32 UH)
{
    f32 h;
    h = (((f32)self->t_fine) - 76800.0f);
    if (h == 0.0f)
        return 0.0f;
    h = ((f32)UH - (((f32)self->calib.dig_H4) * 64.0f + ((f32)self->calib.dig_H5) / 16384.0f * h));
    h = h * (((f32)self->calib.dig_H2) / 65536.0f *
             (1.0f + ((f32)self->calib.dig_H6) / 67108864.0f * h *
                         (1.0f + ((f32)self->calib.dig_H3) / 67108864.0f * h)));
    h = h * (1.0f - ((f32)self->calib.dig_H1) * h / 524288.0f);
    if (h > 100.0f)
        h = 100.0f;
    else if (h < 0.0f)
        h = 0.0f;
    return h;
}

#else

/* 2. 整数计算版  */

static i32 bme280_calc_t_int(bme280_t* self, i32 UT)
{
    i32 v1, v2;
    v1 = ((((UT >> 3) - ((i32)self->calib.dig_T1 << 1))) * ((i32)self->calib.dig_T2)) >> 11;
    v2 = (((((UT >> 4) - ((i32)self->calib.dig_T1)) * ((UT >> 4) - ((i32)self->calib.dig_T1))) >>
           12) *
          ((i32)self->calib.dig_T3)) >>
         14;
    self->t_fine = v1 + v2;
    return ((self->t_fine * 5) + 128) >> 8;   // 结果为 0.01 摄氏度 (例如 5123 = 51.23C)
}

static u32 bme280_calc_p_int(bme280_t* self, i32 UP)
{
#    if BME280_USE_INT64
    i64 v1, v2, p;
    v1 = (i64)self->t_fine - 128000;
    v2 = v1 * v1 * (i64)self->calib.dig_P6;
    v2 = v2 + ((v1 * (i64)self->calib.dig_P5) << 17);
    v2 = v2 + ((i64)self->calib.dig_P4 << 35);
    v1 = ((v1 * v1 * (i64)self->calib.dig_P3) >> 8) + ((v1 * (i64)self->calib.dig_P2) << 12);
    v1 = (((((i64)1) << 47) + v1)) * ((i64)self->calib.dig_P1) >> 33;
    if (v1 == 0)
        return 0;
    p  = 1048576 - UP;
    p  = (((p << 31) - v2) * 3125) / v1;
    v1 = (((i64)self->calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    v2 = (((i64)self->calib.dig_P8) * p) >> 19;
    p  = ((p + v1 + v2) >> 8) + ((i64)self->calib.dig_P7 << 4);
    return (u32)p;   // Q24.8 格式
#    else
    i32 v1, v2;
    u32 p;
    v1 = (((i32)self->t_fine) >> 1) - (i32)64000;
    v2 = (((v1 >> 2) * (v1 >> 2)) >> 11) * ((i32)self->calib.dig_P6);
    v2 = v2 + ((v1 * ((i32)self->calib.dig_P5)) << 1);
    v2 = (v2 >> 2) + (((i32)self->calib.dig_P4) << 16);
    v1 = (((self->calib.dig_P3 * (((v1 >> 2) * (v1 >> 2)) >> 13)) >> 3) +
          ((((i32)self->calib.dig_P2) * v1) >> 1)) >>
         18;
    v1 = (((32768 + v1)) * ((i32)self->calib.dig_P1)) >> 15;
    if (v1 == 0)
        return 0;
    p = (((u32)(((i32)1048576) - UP) - (v2 >> 12))) * 3125;
    if (p < 0x80000000)
        p = (p << 1) / ((u32)v1);
    else
        p = (p / (u32)v1) << 1;
    v1 = (((i32)self->calib.dig_P9) * ((i32)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    v2 = (((i32)(p >> 2)) * ((i32)self->calib.dig_P8)) >> 13;
    p  = (u32)((i32)p + ((v1 + v2 + self->calib.dig_P7) >> 4));
    return p << 8;   // Q24.8 格式
#    endif
}

static u32 bme280_calc_h_int(bme280_t* self, i32 UH)
{
    i32 v1;
    v1 = (self->t_fine - ((i32)76800));
    v1 = (((((UH << 14) - (((i32)self->calib.dig_H4) << 20) - (((i32)self->calib.dig_H5) * v1)) +
            ((i32)16384)) >>
           15) *
          (((((((v1 * ((i32)self->calib.dig_H6)) >> 10) *
               (((v1 * ((i32)self->calib.dig_H3)) >> 11) + ((i32)32768))) >>
              10) +
             ((i32)2097152)) *
                ((i32)self->calib.dig_H2) +
            8192) >>
           14));
    v1 = (v1 - (((((v1 >> 15) * (v1 >> 15)) >> 7) * ((i32)self->calib.dig_H1)) >> 4));
    v1 = (v1 < 0 ? 0 : v1);
    v1 = (v1 > 419430400 ? 419430400 : v1);
    return (u32)(v1 >> 12);   // Q22.10 格式
}

#endif

i32 bme280_init(bme280_t* self, void* hi2c, u16 dev_addr)
{
    if (!self)
        return BME280_ERR_INVALID_PARAM;
    if (!g_bme280_port)
        return BME280_ERR_PORT_NOT_REGISTERED;

    self->hi2c     = hi2c;
    self->dev_addr = dev_addr;

    i32 res = bme280_check(self);
    if (res != BME280_OK)
        return res;

    bme280_reset(self);
    g_bme280_port->delay_ms(10);

    /* 读取校准参数 T1-P9 (24 字节) */
    if (bme280_read_regs(self, BME280_REG_CALIB00, (u8*)&self->calib.dig_T1, 24) != 0)
        return BME280_ERR_I2C_FAIL;

    /* 读取 H1 */
    if (bme280_read_reg(self, BME280_REG_CALIB25, &self->calib.dig_H1) != 0)
        return BME280_ERR_I2C_FAIL;

    /* 读取 H2..H6 (0xE1 - 0xE7, 7 字节) */
    u8 h_buf[7];
    if (bme280_read_regs(self, BME280_REG_CALIB26, h_buf, 7) != 0)
        return BME280_ERR_I2C_FAIL;
    self->calib.dig_H2 = (i16)((((i8)h_buf[1]) << 8) | h_buf[0]);
    self->calib.dig_H3 = h_buf[2];
    self->calib.dig_H4 = (i16)((((i8)h_buf[3]) << 4) | (h_buf[4] & 0x0f));
    self->calib.dig_H5 = (i16)((((i8)h_buf[5]) << 4) | (h_buf[4] >> 4));
    self->calib.dig_H6 = (i8)h_buf[6];

    /* 默认配置: Normal 模式, 16x 采样, 1000ms 等待, Filter 16 */
    res = bme280_config(self,
                        BME280_OSRS_16X,
                        BME280_OSRS_16X,
                        BME280_OSRS_16X,
                        BME280_FILTER_16,
                        BME280_STANDBY_1000MS);
    if (res != BME280_OK)
        return res;

    return bme280_set_mode(self, BME280_MODE_NORMAL);
}

i32 bme280_check(bme280_t* self)
{
    u8 id = 0;
    if (bme280_read_reg(self, BME280_REG_ID, &id) != 0)
        return BME280_ERR_I2C_FAIL;
    if (id != BME280_CHIP_ID) {
        debug_print("[bme280] error: device not found, id:0x%X\n", id);
        return BME280_ERR_DEVICE_NOT_FOUND;
    }
    return BME280_OK;
}

i32 bme280_get_chip_id(bme280_t* self, u8* id)
{
    if (!id)
        return BME280_ERR_INVALID_PARAM;
    return bme280_read_reg(self, BME280_REG_ID, id);
}

bool bme280_is_measuring(bme280_t* self)
{
    u8 status = 0;
    if (bme280_read_reg(self, BME280_REG_STATUS, &status) != 0)
        return false;
    return (status & 0x08) != 0;
}

bool bme280_is_updating(bme280_t* self)
{
    u8 status = 0;
    if (bme280_read_reg(self, BME280_REG_STATUS, &status) != 0)
        return false;
    return (status & 0x01) != 0;
}

i32 bme280_reset(bme280_t* self)
{
    return bme280_write_reg(self, BME280_REG_RESET, BME280_SOFT_RESET_VAL);
}

i32 bme280_config(bme280_t* self, bme280_osrs_t osrs_t, bme280_osrs_t osrs_p, bme280_osrs_t osrs_h,
                  bme280_filter_t filter, bme280_standby_t standby)
{
    i32 res;

    /* 写入湿度过采样 (0xF2) */
    res = bme280_write_reg(self, BME280_REG_CTRL_HUM, osrs_h & 0x07);
    if (res != 0)
        return BME280_ERR_I2C_FAIL;

    /* 写入配置寄存器 (0xF5) */
    res = bme280_write_reg(self, BME280_REG_CONFIG, (standby << 5) | (filter << 2));
    if (res != 0)
        return BME280_ERR_I2C_FAIL;

    /* 写入测量控制寄存器 (0xF4)
       注意：写入此寄存器会使 CTRL_HUM 的更改生效 */
    u8 ctrl_meas;
    if (bme280_read_reg(self, BME280_REG_CTRL_MEAS, &ctrl_meas) != 0)
        return BME280_ERR_I2C_FAIL;
    ctrl_meas = (osrs_t << 5) | (osrs_p << 2) | (ctrl_meas & 0x03);
    res       = bme280_write_reg(self, BME280_REG_CTRL_MEAS, ctrl_meas);
    if (res != 0)
        return BME280_ERR_I2C_FAIL;

    return BME280_OK;
}

i32 bme280_set_mode(bme280_t* self, bme280_mode_t mode)
{
    u8 reg;
    if (bme280_read_reg(self, BME280_REG_CTRL_MEAS, &reg) != 0)
        return BME280_ERR_I2C_FAIL;
    reg = (reg & ~0x03) | (mode & 0x03);
    return bme280_write_reg(self, BME280_REG_CTRL_MEAS, reg);
}

i32 bme280_set_osrs_t(bme280_t* self, bme280_osrs_t osrs)
{
    u8 reg;
    if (bme280_read_reg(self, BME280_REG_CTRL_MEAS, &reg) != 0)
        return BME280_ERR_I2C_FAIL;
    reg = (reg & ~0xE0) | ((osrs & 0x07) << 5);
    return bme280_write_reg(self, BME280_REG_CTRL_MEAS, reg);
}

i32 bme280_set_osrs_p(bme280_t* self, bme280_osrs_t osrs)
{
    u8 reg;
    if (bme280_read_reg(self, BME280_REG_CTRL_MEAS, &reg) != 0)
        return BME280_ERR_I2C_FAIL;
    reg = (reg & ~0x1C) | ((osrs & 0x07) << 2);
    return bme280_write_reg(self, BME280_REG_CTRL_MEAS, reg);
}

i32 bme280_set_osrs_h(bme280_t* self, bme280_osrs_t osrs)
{
    i32 res = bme280_write_reg(self, BME280_REG_CTRL_HUM, osrs & 0x07);
    if (res != 0)
        return res;
    /* 必须写一下 CTRL_MEAS 才能使 CTRL_HUM 生效 */
    u8 reg;
    if (bme280_read_reg(self, BME280_REG_CTRL_MEAS, &reg) != 0)
        return BME280_ERR_I2C_FAIL;
    return bme280_write_reg(self, BME280_REG_CTRL_MEAS, reg);
}

i32 bme280_set_filter(bme280_t* self, bme280_filter_t filter)
{
    u8 reg;
    if (bme280_read_reg(self, BME280_REG_CONFIG, &reg) != 0)
        return BME280_ERR_I2C_FAIL;
    reg = (reg & ~0x1C) | ((filter & 0x07) << 2);
    return bme280_write_reg(self, BME280_REG_CONFIG, reg);
}

i32 bme280_set_standby(bme280_t* self, bme280_standby_t standby)
{
    u8 reg;
    if (bme280_read_reg(self, BME280_REG_CONFIG, &reg) != 0)
        return BME280_ERR_I2C_FAIL;
    reg = (reg & ~0xE0) | ((standby & 0x07) << 5);
    return bme280_write_reg(self, BME280_REG_CONFIG, reg);
}

i32 bme280_read_raw_press(bme280_t* self, i32* up)
{
    u8 buf[3];
    if (bme280_read_regs(self, BME280_REG_PRESS_MSB, buf, 3) != 0)
        return BME280_ERR_I2C_FAIL;
    if (up)
        *up = (i32)((buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4));
    return BME280_OK;
}

i32 bme280_read_raw_temp(bme280_t* self, i32* ut)
{
    u8 buf[3];
    if (bme280_read_regs(self, BME280_REG_TEMP_MSB, buf, 3) != 0)
        return BME280_ERR_I2C_FAIL;
    if (ut)
        *ut = (i32)((buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4));
    return BME280_OK;
}

i32 bme280_read_raw_hum(bme280_t* self, i32* uh)
{
    u8 buf[2];
    if (bme280_read_regs(self, BME280_REG_HUM_MSB, buf, 2) != 0)
        return BME280_ERR_I2C_FAIL;
    if (uh)
        *uh = (i32)((buf[0] << 8) | buf[1]);
    return BME280_OK;
}

i32 bme280_read_all(bme280_t* self, f32* temperature, f32* pressure, f32* humidity)
{
    u8 buf[8];
    if (bme280_read_regs(self, BME280_REG_PRESS_MSB, buf, 8) != 0)
        return BME280_ERR_I2C_FAIL;

    i32 up = (i32)((buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4));
    i32 ut = (i32)((buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4));
    i32 uh = (i32)((buf[6] << 8) | buf[7]);

    if (ut == 0x80000) {
        if (temperature)
            *temperature = 0.0f;
        if (pressure)
            *pressure = 0.0f;
        if (humidity)
            *humidity = 0.0f;
        return BME280_OK;
    }

#if BME280_USE_FLOAT
    f32 t = bme280_calc_tf(self, ut);
    if (temperature)
        *temperature = t;
    if (pressure)
        *pressure = bme280_calc_pf(self, up);
    if (humidity)
        *humidity = bme280_calc_hf(self, uh);
#else
    /* 整数版计算并将结果转换为接口定义的 f32 */
    i32 t_int = bme280_calc_t_int(self, ut);
    u32 p_int = bme280_calc_p_int(self, up);
    u32 h_int = bme280_calc_h_int(self, uh);

    if (temperature)
        *temperature = (f32)t_int / 100.0f;
    if (pressure)
        *pressure = (f32)p_int / 256.0f;
    if (humidity)
        *humidity = (f32)h_int / 1024.0f;
#endif

    return BME280_OK;
}
f32 bme280_pa_to_mmhg(f32 pa)
{
    /* 1 Pa = 0.00750061683 mmHg */
    return pa * 0.00750061683f;
}

u32 bme280_pa_to_mmhg_int(u32 pq24_8)
{

    /* PQ24_8 是 CalcP 输出的 Q24.8 格式压力 */
    u32 p_mmhg = (pq24_8 >> 6) * BME_MMHG_Q0_20;
    return ((p_mmhg >> 22) * 1000) + ((((p_mmhg << 10) >> 18) * 61039) / 1000000);
}

i32 bme280_pa_to_alt_int(u32 pa)
{

    i32 p1    = (i32)pa - 101325;
    i32 alt_i = 0;
    /* 第一项: -0.0832546 * 1000 * 256 -> 21313 */
    alt_i -= (p1 * 21313) >> 8;
    /* 第二项: 3.32651E-7 * 1000 * 256 -> 22 */
    alt_i += (((p1 * p1) >> 8) * 22) >> 8;
    return alt_i;
}

f32 bme280_pa_to_alt(f32 pa)
{
    /**
     * @brief 使用泰勒级数近似计算海拔 (巴罗米特公式)
     * @details 在 101325Pa (海平面) 附近展开:
     * h = -0.0832546 * (P-101325) + 3.32651e-7 * (P-101325)^2
     * 该近似在 +/- 2km 范围内非常精确。
     */
    f32 p1 = pa - 101325.0f;
    f32 p2 = p1 * p1;
    return ((-0.0832546f * p1) + (3.32651E-7f * p2));
}
