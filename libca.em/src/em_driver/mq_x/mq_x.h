 /// @file mq_x.h
 /// @author Canrad
 /// @brief MQ 系列气体传感器驱动（如 MQ-3, MQ-135 等）
 /// 参考文档如下
 ///   mq_2 烟雾传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-2-sensor.html
 ///   mq_3 酒精检测传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-3-sensor.html
 ///   mq_4 甲烷检测传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-4-sensor.html
 ///   mq_5 液化气检测传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-5-sensor.html
 ///   mq_6 丙烷检测传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-6-sensor.html
 ///   mq_7 一氧化碳检测传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-7-sensor.html
 ///   mq_8 氢气检测传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-8-sensor.html
 ///   mq_9 可燃气体检测传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-9-sensor.html
 ///   mq_135 空气质量传感器 https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/mq-135-sensor.html
 /// @version 0.1
 /// @date 2026-01-23
 /// @update 0.2 添加extern外部依赖注入模式
 ///
 /// @copyright Copyright (c) 2026
 ///
#ifndef LIBCA_EM_DRIVER_MQ_X_H
#define LIBCA_EM_DRIVER_MQ_X_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_MQ_X_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_MQ_X_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_MQ_X_PORT_MODE
#define LIBCA_MQ_X_PORT_MODE LIBCA_MQ_X_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MQ_X_ENABLE_MQ2
#define MQ_X_ENABLE_MQ2 0
#endif

#ifndef MQ_X_ENABLE_MQ3
#define MQ_X_ENABLE_MQ3 0
#endif

#ifndef MQ_X_ENABLE_MQ4
#define MQ_X_ENABLE_MQ4 0
#endif

#ifndef MQ_X_ENABLE_MQ5
#define MQ_X_ENABLE_MQ5 0
#endif

#ifndef MQ_X_ENABLE_MQ6
#define MQ_X_ENABLE_MQ6 0
#endif

#ifndef MQ_X_ENABLE_MQ7
#define MQ_X_ENABLE_MQ7 0
#endif

#ifndef MQ_X_ENABLE_MQ8
#define MQ_X_ENABLE_MQ8 0
#endif

#ifndef MQ_X_ENABLE_MQ9
#define MQ_X_ENABLE_MQ9 0
#endif

#ifndef MQ_X_ENABLE_MQ135
#define MQ_X_ENABLE_MQ135 0
#endif

#if (LIBCA_MQ_X_PORT_MODE == LIBCA_MQ_X_PORT_MODE_EXTERN)
 /// @brief 读取ADC值
 /// @param adc ADC句柄
 /// @param channel 通道号
 /// @return ADC值
extern u16 port_mqx_read_adc(void* adc, u8 channel);
 /// @brief 读取数字引脚
 /// @param gpio GPIO句柄
 /// @param pin 引脚号
 /// @return 引脚电平
extern u8 port_mqx_read_pin(void* gpio, u16 pin);

#elif (LIBCA_MQ_X_PORT_MODE == LIBCA_MQ_X_PORT_MODE_DYNAMIC)
typedef struct mqx_port {
    u16 (*read_adc)(void* adc, u8 channel);  // 读取ADC值
    u8  (*read_pin)(void* gpio, u16 pin);    // 读取数字引脚
} mqx_port_t;
void mqx_bind_port(const mqx_port_t* port);
bool mqx_port_is_registered(void);

#else
#error "Invalid MQ_X port mode"
#endif

 /// @brief mqx 对象结构体
typedef struct mqx {
    void* adc_hdl;
    u8    adc_ch;
    void* do_gpio;
    u16   do_pin;

    u16 adc_max; // ADC 最大值，例如 12 位为 4095
    u8  samples; // 采样平均次数
} mqx_t;

 /// @brief 初始化 mqx 对象
 ///
 /// @param self 对象指针
 /// @param adc_hdl ADC 句柄
 /// @param adc_ch ADC 通道
 /// @param do_gpio DO 引脚的 GPIO 句柄
 /// @param do_pin DO 引脚编号
 /// @param adc_resolution ADC 分辨率位数（如 12）
 /// @param samples 采样平均次数
void mqx_init(mqx_t* self, void* adc_hdl, u8 adc_ch, void* do_gpio, u16 do_pin, u8 adc_resolution, u8 samples);

 /// @brief 获取 ADC 平均值
 ///
 /// @param self 对象指针
 /// @return u16 ADC 原始值
u16 mqx_get_adc(mqx_t* self);

 /// @brief 获取气体浓度百分比值 (0-100)
 ///
 /// @param self 对象指针
 /// @return u8 百分比值 (0-100)
u8 mqx_get_percentage(mqx_t* self);

 /// @brief 获取数字输出引脚状态
 ///
 /// @param self 对象指针
 /// @return u8 引脚电平
u8 mqx_get_do_state(mqx_t* self);

#if MQ_X_ENABLE_MQ2
// 定义别名
typedef mqx_t mq2;
#endif

#if MQ_X_ENABLE_MQ3
typedef mqx_t mq3;
#endif

#if MQ_X_ENABLE_MQ4
typedef mqx_t mq4;
#endif

#if MQ_X_ENABLE_MQ5
typedef mqx_t mq5;
#endif

#if MQ_X_ENABLE_MQ6
typedef mqx_t mq6;
#endif

#if MQ_X_ENABLE_MQ7
typedef mqx_t mq7;
#endif

#if MQ_X_ENABLE_MQ8
typedef mqx_t mq8;
#endif

#if MQ_X_ENABLE_MQ9
typedef mqx_t mq9;
#endif

#if MQ_X_ENABLE_MQ135
typedef mqx_t mq135;
#endif

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_DRIVER_MQ_X_H
