/**
 * @file bmp280.h
 * @author canrad (1517807724@qq.com)
 * @brief BMP280 气压计传感器驱动
 * @version 0.1
 * @date 2026-01-22
 */

#ifndef LIBCA_EM_DRIVER_BMP280_H
#define LIBCA_EM_DRIVER_BMP280_H

#include "em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- 配置选项 --- */

/**
 * @brief 补偿计算模式
 * 1: 使用 64 位整数计算 (默认, 精度较高)
 * 2: 使用浮点数计算 (精度最高, 建议有 FPU 时使用)
 */
#ifndef BMP280_CALC_MODE
#define BMP280_CALC_MODE 2
#endif

/* --- 硬件接口 (Port Binding) --- */

/**
 * @brief BMP280 软件端口绑定
 */
typedef struct bmp280_port {
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    void (*delay_ms)(u32 ms);
} bmp280_port_t;

/**
 * @brief 绑定硬件端口
 */
void bmp280_bind_port(const bmp280_port_t* port);

/**
 * @brief 检查硬件端口是否已注册
 */
bool bmp280_port_is_registered(void);

/* --- 寄存器定义 --- */


#define BMP280_REG_CALIB                0x88 // 校准数据起始地址 (24 字节)
#define BMP280_REG_ID                   0xD0 // 芯片 ID
#define BMP280_REG_RESET                0xE0 // 软件复位
#define BMP280_REG_STATUS               0xF3 // 状态寄存器
#define BMP280_REG_CTRL_MEAS            0xF4 // 测量控制
#define BMP280_REG_CONFIG               0xF5 // 配置寄存器
#define BMP280_REG_PRESS_MSB            0xF7 // 压力数据 MSB
#define BMP280_REG_TEMP_MSB             0xFA // 温度数据 MSB

#define BMP280_CHIP_ID                  0x58
#define BMP280_SOFT_RESET_VAL           0xB6

/* --- 枚举与结构体 --- */

/**
 * @brief BMP280 校准参数
 */
typedef struct {
    u16 dig_T1;
    i16 dig_T2;
    i16 dig_T3;
    u16 dig_P1;
    i16 dig_P2;
    i16 dig_P3;
    i16 dig_P4;
    i16 dig_P5;
    i16 dig_P6;
    i16 dig_P7;
    i16 dig_P8;
    i16 dig_P9;
    i32 t_fine; // 中间计算值
} bmp280_calib_t;

/**
 * @brief BMP280 设备实例
 */
typedef struct {
    void* hi2c;             // I2C 句柄
    u16 dev_addr;           // 设备 8 位地址
    bmp280_calib_t calib;   // 校准数据
} bmp280_t;

/**
 * @brief 测量模式
 */
typedef enum {
    BMP280_MODE_SLEEP  = 0x00,
    BMP280_MODE_FORCED = 0x01,
    BMP280_MODE_NORMAL = 0x03
} bmp280_mode_t;

/**
 * @brief 过采样率
 */
typedef enum {
    BMP280_OSRS_SKIP = 0x00,
    BMP280_OSRS_1X   = 0x01,
    BMP280_OSRS_2X   = 0x02,
    BMP280_OSRS_4X   = 0x03,
    BMP280_OSRS_8X   = 0x04,
    BMP280_OSRS_16X  = 0x05
} bmp280_osrs_t;

/**
 * @brief 滤波器系数
 */
typedef enum {
    BMP280_FILTER_OFF = 0x00,
    BMP280_FILTER_2   = 0x01,
    BMP280_FILTER_4   = 0x02,
    BMP280_FILTER_8   = 0x03,
    BMP280_FILTER_16  = 0x04
} bmp280_filter_t;

/**
 * @brief 正常模式下的等待时间
 */
typedef enum {
    BMP280_STBY_0_5MS  = 0x00,
    BMP280_STBY_62_5MS = 0x01,
    BMP280_STBY_125MS  = 0x02,
    BMP280_STBY_250MS  = 0x03,
    BMP280_STBY_500MS  = 0x04,
    BMP280_STBY_1000MS = 0x05,
    BMP280_STBY_2000MS = 0x06,
    BMP280_STBY_4000MS = 0x07
} bmp280_standby_t;

/* --- 错误码 --- */
#define BMP280_OK                          0
#define BMP280_ERR_PORT_NOT_REGISTERED    (-1)
#define BMP280_ERR_I2C_FAIL               (-2)
#define BMP280_ERR_DEVICE_NOT_FOUND       (-3)
#define BMP280_ERR_INVALID_PARAM          (-4)

/* --- 接口函数 --- */

/**
 * @brief 初始化 BMP280
 */
i32 bmp280_init(bmp280_t* self, void* hi2c, u16 dev_addr);

/**
 * @brief 检查设备是否在线
 */
i32 bmp280_check(bmp280_t* self);

/**
 * @brief 软件复位
 */
i32 bmp280_reset(bmp280_t* self);

/**
 * @brief 配置传感器参数
 */
i32 bmp280_config(bmp280_t* self, bmp280_osrs_t osrs_t, bmp280_osrs_t osrs_p, 
                  bmp280_filter_t filter, bmp280_standby_t standby);

/**
 * @brief 设置工作模式
 */
i32 bmp280_set_mode(bmp280_t* self, bmp280_mode_t mode);

/**
 * @brief 是否正在测量
 */
bool bmp280_is_measuring(bmp280_t* self);

/**
 * @brief 读取温度
 * @param temperature 摄氏度
 */
i32 bmp280_read_temp(bmp280_t* self, f32* temperature);

/**
 * @brief 读取压力
 * @param pressure 帕斯卡 (Pa)
 */
i32 bmp280_read_press(bmp280_t* self, f32* pressure);

/**
 * @brief 同时读取温度和压力
 */
i32 bmp280_read_all(bmp280_t* self, f32* temperature, f32* pressure);

/* --- 工具函数 (Stateless) --- */

/**
 * @brief 帕斯卡转毫米汞柱
 */
f32 bmp280_pa_to_mmhg(f32 pa);

/**
 * @brief 帕斯卡转海拔
 */
f32 bmp280_pa_to_alt(f32 pa);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_BMP280_H
