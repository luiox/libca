/**
 * @file ads1115.h
 * @author canrad (1517807724@qq.com)
 * @brief ADS1115 16位ADC驱动
 * @version 0.1
 * @date 2026-01-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_ADS1115_H
#define LIBCA_EM_DRIVER_ADS1115_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADS1115 错误码 */
#define ADS1115_OK                      0
#define ADS1115_ERR_PORT_NOT_REGISTERED (-1)
#define ADS1115_ERR_I2C                 (-2)
#define ADS1115_ERR_TIMEOUT             (-3)

// port
typedef struct ads1115_port {
    /**
     * @brief I2C 写操作
     * 
     * @param hi2c I2C 句柄
     * @param dev_addr 设备 7 位地址
     * @param reg_addr 寄存器地址
     * @param data 数据缓冲区
     * @param size 数据长度
     * @return i32 返回 0 表示成功，非 0 表示失败
     */
    i32 (*i2c_write)(void* hi2c, u8 dev_addr, u8 reg_addr, const u8* data, u16 size);

    /**
     * @brief I2C 读操作
     * 
     * @param hi2c I2C 句柄
     * @param dev_addr 设备 7 位地址
     * @param reg_addr 寄存器地址
     * @param data 数据缓冲区
     * @param size 数据长度
     * @return i32 返回 0 表示成功，非 0 表示失败
     */
    i32 (*i2c_read)(void* hi2c, u8 dev_addr, u8 reg_addr, u8* data, u16 size);

    /**
     * @brief 毫秒延时
     * 
     * @param ms 毫秒数
     */
    void (*delay_ms)(u32 ms);
} ads1115_port_t;

/**
 * @brief 绑定硬件接口
 * 
 * @param port 接口结构体
 */
void ads1115_bind_port(const ads1115_port_t* port);

/**
 * @brief 检查接口是否已注册
 * 
 * @return bool true 为已注册
 */
bool ads1115_port_is_registered(void);

/**
 * @brief 输入通道枚举
 */
typedef enum ads1115_mux_enum{
    ADS1115_MUX_DIFF_0_1 = 0x00, // AIN0 - AIN1 (默认)
    ADS1115_MUX_DIFF_0_3 = 0x01, // AIN0 - AIN3
    ADS1115_MUX_DIFF_1_3 = 0x02, // AIN1 - AIN3
    ADS1115_MUX_DIFF_2_3 = 0x03, // AIN2 - AIN3
    ADS1115_MUX_SINGLE_0  = 0x04, // AIN0 - GND
    ADS1115_MUX_SINGLE_1  = 0x05, // AIN1 - GND
    ADS1115_MUX_SINGLE_2  = 0x06, // AIN2 - GND
    ADS1115_MUX_SINGLE_3  = 0x07  // AIN3 - GND
} ads1115_mux;

/**
 * @brief 增益量程枚举 (PGA)
 */
typedef enum ads1115_pga_enum{
    ADS1115_PGA_6144 = 0x00, // ±6.144V
    ADS1115_PGA_4096 = 0x01, // ±4.096V
    ADS1115_PGA_2048 = 0x02, // ±2.048V (默认)
    ADS1115_PGA_1024 = 0x03, // ±1.024V
    ADS1115_PGA_0512 = 0x04, // ±0.512V
    ADS1115_PGA_0256 = 0x05  // ±0.256V
} ads1115_pga;

/**
 * @brief 工作模式枚举
 */
typedef enum ads1115_mode_enum {
    ADS1115_MODE_CONTINUOUS = 0x00, // 连续转换
    ADS1115_MODE_SINGLE     = 0x01  // 单次转换 (默认)
} ads1115_mode;

/**
 * @brief 采样率枚举 (SPS)
 */
typedef enum ads1115_rate_enum {
    ADS1115_RATE_8   = 0x00, // 8 SPS
    ADS1115_RATE_16  = 0x01, // 16 SPS
    ADS1115_RATE_32  = 0x02, // 32 SPS
    ADS1115_RATE_64  = 0x03, // 64 SPS
    ADS1115_RATE_128 = 0x04, // 128 SPS (默认)
    ADS1115_RATE_250 = 0x05, // 250 SPS
    ADS1115_RATE_475 = 0x06, // 475 SPS
    ADS1115_RATE_860 = 0x07  // 860 SPS
} ads1115_rate;

/**
 * @brief ADS1115 对象结构体
 */
typedef struct ads1115 {
    void* hi2c;
    u8    dev_addr;   // 7 位地址
    f32   gain_lsb;   // 当前增益对应的 LSB 电压值
} ads1115_t;

/**
 * @brief 初始化 ADS1115 对象
 * 
 * @param self 对象指针
 * @param hi2c I2C 句柄
 * @param dev_addr 设备 7 位地址（通常为 0x48）
 */
void ads1115_init(ads1115_t* self, void* hi2c, u8 dev_addr);

/**
 * @brief 配置 ADS1115
 * 
 * @param self 对象指针
 * @param mux 输入通道选择
 * @param pga 增益量程选择
 * @param mode 工作模式选择
 * @param rate 采样速率选择
 * @return i32 错误码
 */
i32 ads1115_config(ads1115_t* self, ads1115_mux mux, ads1115_pga pga, ads1115_mode mode, ads1115_rate rate);

/**
 * @brief 读取 ADC 电压值
 * 
 * @param self 对象指针
 * @param voltage 返回的电压值指针
 * @return i32 错误码
 */
i32 ads1115_read_voltage(ads1115_t* self, f32* voltage);

/**
 * @brief 读取 ADC 原始原始值
 * 
 * @param self 对象指针
 * @param raw_val 原始值指针
 * @return i32 错误码
 */
i32 ads1115_read_raw(ads1115_t* self, i16* raw_val);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_DRIVER_ADS1115_H
