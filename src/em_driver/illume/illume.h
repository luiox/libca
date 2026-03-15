/**
 * @file illume.h
 * @author canrad (1517807724@qq.com)
 * 参考文档：https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/photoresistance-sensor.html
 * @brief 光敏电阻传感器驱动
 * @version 0.1
 * @date 2026-01-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_ILLUME_H
#define LIBCA_EM_DRIVER_ILLUME_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_ILLUME_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_ILLUME_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_ILLUME_PORT_MODE
#define LIBCA_ILLUME_PORT_MODE LIBCA_ILLUME_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

// port
typedef struct illume_port {
    /**
     * @brief 读取 ADC 原始值
     * 
     * @param adc ADC 句柄
     * @param channel 通道
     * @return u16 ADC 原始值
     */
    u16 (*read_adc)(void* adc, u8 channel);

    /**
     * @brief 读取 IO 引脚电平
     * 
     * @param gpio GPIO 句柄
     * @param pin 引脚编号
     * @return u8 电平状态
     */
    u8 (*read_pin)(void* gpio, u16 pin);
} illume_port_t;

/**
 * @brief 外部隐式注入的 port 函数表（由 port_illume.c 提供）
 */
extern const illume_port_t g_illume_port_extern;


/**
 * @brief 绑定硬件接口
 * 
 * @param port 接口结构体
 */
void illume_bind_port(const illume_port_t* port);

/**
 * @brief 检查接口是否已注册
 * 
 * @return bool true 为已注册
 */
bool illume_port_is_registered(void);

/**
 * @brief Illume 对象结构体
 */
typedef struct illume {
    void* adc_hdl;
    u8    adc_ch;
    void* do_gpio;
    u16   do_pin;

    u16 adc_max; // ADC 最大值，例如 12 位为 4095
} illume_t;

/**
 * @brief 初始化 Illume 对象
 * 
 * @param self 对象指针
 * @param adc_hdl ADC 句柄
 * @param adc_ch ADC 通道
 * @param do_gpio DO 引脚的 GPIO 句柄
 * @param do_pin DO 引脚编号
 * @param adc_resolution ADC 分辨率位数 (有效范围 1-16, 如 12)
 */
void illume_init(illume_t* self, void* adc_hdl, u8 adc_ch, void* do_gpio, u16 do_pin, u8 adc_resolution);

/**
 * @brief 获取光照强度百分比值 (0-100)
 * 
 * @param self 对象指针
 * @param count 采集次数用于求平均
 * @return u8 百分比值 (0-100)
 */
u8 illume_get_percentage(illume_t* self, u8 count);

/**
 * @brief 获取数字输出引脚状态
 * 
 * @param self 对象指针
 * @return u8 1=亮, 0=暗 (取决于模块自身的 DO 逻辑)
 */
u8 illume_get_do_state(illume_t* self);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_DRIVER_ILLUME_H
