/**
 * @file bme280.h
 * @author canrad (1517807724@qq.com)
 * @brief BME280 温湿度压力传感器驱动
 * 参考文章：https://blog.csdn.net/zhe_boy_is_z/article/details/120207878
 * @version 0.1
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_BME280_H
#define LIBCA_EM_DRIVER_BME280_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_BME280_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_BME280_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_BME280_PORT_MODE
#define LIBCA_BME280_PORT_MODE LIBCA_BME280_PORT_MODE_EXTERN
#endif

#if (LIBCA_BME280_PORT_MODE == LIBCA_BME280_PORT_MODE_EXTERN)

/**
 * @brief I2C 写操作
 * @param hi2c I2C 句柄
 * @param dev_addr 设备地址
 * @param mem_addr 内存地址
 * @param mem_addr_size 地址字节数
 * @param data 数据缓冲区
 * @param data_size 数据长度
 * @param timeout 超时（ms）
 * @return 0 表示成功
 */
extern i32 port_bme280_i2c_write(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size,
                                 u32 timeout);

/**
 * @brief I2C 读操作
 * @param hi2c I2C 句柄
 * @param dev_addr 设备地址
 * @param mem_addr 内存地址
 * @param mem_addr_size 地址字节数
 * @param data 数据缓冲区
 * @param data_size 数据长度
 * @param timeout 超时（ms）
 * @return 0 表示成功
 */
extern i32 port_bme280_i2c_read(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size,
                                u32 timeout);

/**
 * @brief 毫秒延时
 * @param ms 延时时间（ms）
 */
extern void port_bme280_delay_ms(u32 ms);

#elif (LIBCA_BME280_PORT_MODE == LIBCA_BME280_PORT_MODE_DYNAMIC)

typedef struct bme280_port {
    // i2c写函数
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size,
                     u32 timeout);
    // i2c读函数
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size,
                    u32 timeout);
    // 毫秒延时函数
    void (*delay_ms)(u32 ms);
} bme280_port_t;

void bme280_bind_port(const bme280_port_t* port);
bool bme280_port_is_registered(void);

#else
#error "Invalid BME280 port mode"
#endif

// 错误码
#define BME280_OK                          0
#define BME280_ERR_I2C_FAIL               (-2)
#define BME280_ERR_DEVICE_NOT_FOUND       (-3)
#define BME280_ERR_INVALID_PARAM          (-4)

/**
 * @brief BME280 校准参数
 */
typedef struct bme280_calibration {
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
    u8  dig_H1;
    i16 dig_H2;
    u8  dig_H3;
    i16 dig_H4;
    i16 dig_H5;
    i8  dig_H6;
} bme280_calibration_t;

/**
 * @brief BME280 设备实例
 */
typedef struct bme280 {
    void* hi2c;                  // I2C 句柄
    u16 dev_addr;                // 设备地址 (8位)
    bme280_calibration_t calib;  // 校准数据
    i32 t_fine;                  // 用于压力和湿度计算的中间温度值
} bme280_t;

/**
 * @brief BME280 工作模式
 */
typedef enum {
    BME280_MODE_SLEEP  = 0x00,
    BME280_MODE_FORCED = 0x01,
    BME280_MODE_NORMAL = 0x03
} bme280_mode_t;

/**
 * @brief BME280 过采样率
 */
typedef enum {
    BME280_OSRS_SKIP = 0x00,
    BME280_OSRS_1X   = 0x01,
    BME280_OSRS_2X   = 0x02,
    BME280_OSRS_4X   = 0x03,
    BME280_OSRS_8X   = 0x04,
    BME280_OSRS_16X  = 0x05
} bme280_osrs_t;

/**
 * @brief BME280 IIR 滤波器系数
 */
typedef enum {
    BME280_FILTER_OFF = 0x00,
    BME280_FILTER_2   = 0x01,
    BME280_FILTER_4   = 0x02,
    BME280_FILTER_8   = 0x03,
    BME280_FILTER_16  = 0x04
} bme280_filter_t;

/**
 * @brief BME280 正常模式下的等待时间
 */
typedef enum {
    BME280_STANDBY_0_5MS   = 0x00,
    BME280_STANDBY_62_5MS  = 0x01,
    BME280_STANDBY_125MS   = 0x02,
    BME280_STANDBY_250MS   = 0x03,
    BME280_STANDBY_500MS   = 0x04,
    BME280_STANDBY_1000MS  = 0x05,
    BME280_STANDBY_10MS    = 0x06,
    BME280_STANDBY_20MS    = 0x07
} bme280_standby_t;

// 常用设备地址
#define BME280_I2C_ADDR_VCC (0x77 << 1)
#define BME280_I2C_ADDR_GND (0x76 << 1)

/**
 * @brief 初始化 BME280
 * @param self 实例指针
 * @param hi2c I2C 句柄
 * @param dev_addr 设备 8 位地址
 * @return i32 状态码
 */
i32 bme280_init(bme280_t* self, void* hi2c, u16 dev_addr);

/**
 * @brief 检查 BME280 是否在线
 * @param self 实例指针
 * @return i32 状态码
 */
i32 bme280_check(bme280_t* self);

/**
 * @brief 软件复位
 * @param self 实例指针
 * @return i32 状态码
 */
i32 bme280_reset(bme280_t* self);

/**
 * @brief 获取芯片 ID (BME280 固定为 0x60)
 * @param self 实例指针
 * @param id ID 存储地址
 * @return i32 状态码
 */
i32 bme280_get_chip_id(bme280_t* self, u8* id);

/**
 * @brief 检查传感器是否正在测量
 * @param self 实例指针
 * @return true 正在测量, false 空闲
 */
bool bme280_is_measuring(bme280_t* self);

/**
 * @brief 检查 NVM 图像数据是否正在更新
 * @param self 实例指针
 * @return true 正在更新, false 完成
 */
bool bme280_is_updating(bme280_t* self);

/**
 * @brief 配置 BME280
 * @param self 实例指针
 * @param osrs_t 温度过采样
 * @param osrs_p 压力过采样
 * @param osrs_h 湿度过采样
 * @param filter 滤波器系数
 * @param standby 等待时间
 * @return i32 状态码
 */
i32 bme280_config(bme280_t* self, bme280_osrs_t osrs_t, bme280_osrs_t osrs_p, bme280_osrs_t osrs_h, 
                  bme280_filter_t filter, bme280_standby_t standby);

/**
 * @brief 设置工作模式
 * @param self 实例指针
 * @param mode 工作模式
 * @return i32 状态码
 */
i32 bme280_set_mode(bme280_t* self, bme280_mode_t mode);

/**
 * @brief 设置温度过采样
 * @param self 实例指针
 * @param osrs 过采样率
 * @return i32 状态码
 */
i32 bme280_set_osrs_t(bme280_t* self, bme280_osrs_t osrs);

/**
 * @brief 设置压力过采样
 * @param self 实例指针
 * @param osrs 过采样率
 * @return i32 状态码
 */
i32 bme280_set_osrs_p(bme280_t* self, bme280_osrs_t osrs);

/**
 * @brief 设置湿度过采样
 * @param self 实例指针
 * @param osrs 过采样率
 * @return i32 状态码
 */
i32 bme280_set_osrs_h(bme280_t* self, bme280_osrs_t osrs);

/**
 * @brief 设置滤波器系数
 * @param self 实例指针
 * @param filter 滤波器系数
 * @return i32 状态码
 */
i32 bme280_set_filter(bme280_t* self, bme280_filter_t filter);

/**
 * @brief 设置待机时间
 * @param self 实例指针
 * @param standby 待机时间
 * @return i32 状态码
 */
i32 bme280_set_standby(bme280_t* self, bme280_standby_t standby);

/**
 * @brief 读取原始压力数据
 * @param self 实例指针
 * @param up 压力存储地址
 * @return i32 状态码
 */
i32 bme280_read_raw_press(bme280_t* self, i32* up);

/**
 * @brief 读取原始温度数据
 * @param self 实例指针
 * @param ut 温度存储地址
 * @return i32 状态码
 */
i32 bme280_read_raw_temp(bme280_t* self, i32* ut);

/**
 * @brief 读取原始湿度数据
 * @param self 实例指针
 * @param uh 湿度存储地址
 * @return i32 状态码
 */
i32 bme280_read_raw_hum(bme280_t* self, i32* uh);

/**
 * @brief 读取所有传感器数据
 * @param self 实例指针
 * @param temperature 温度指针 (单位: 摄氏度)
 * @param pressure 气压指针 (单位: 帕斯卡)
 * @param humidity 湿度指针 (单位: %RH)
 * @return i32 状态码
 */
i32 bme280_read_all(bme280_t* self, f32* temperature, f32* pressure, f32* humidity);

/**
 * @brief 气压单位转换: 帕斯卡转毫米汞柱
 * @param pa 帕斯卡
 * @return f32 毫米汞柱
 */
f32 bme280_pa_to_mmhg(f32 pa);

/**
 * @brief 气压转海拔 (浮点版)
 * @param pa 帕斯卡
 * @return f32 海拔 (米)
 */
f32 bme280_pa_to_alt(f32 pa);

/**
 * @brief 气压单位转换: 帕斯卡 (Q24.8) 转毫米汞柱 (整数版)
 * @param pq24_8 气压 (Q24.8 格式)
 * @return u32 毫米汞柱 (0.001 mmHg)
 */
u32 bme280_pa_to_mmhg_int(u32 pq24_8);

/**
 * @brief 气压转海拔 (整数版)
 * @param pa 帕斯卡
 * @return i32 海拔 (毫米)
 */
i32 bme280_pa_to_alt_int(u32 pa);

#endif // LIBCA_EM_DRIVER_BME280_H
