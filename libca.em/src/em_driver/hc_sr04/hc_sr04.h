/// @file hc_sr04.h
/// @author canrad (1517807724@qq.com)
/// @brief HC-SR04 超声波测距驱动
/// @version 0.1
/// @date 2026-01-22
/// @update 0.2 添加extern外部依赖注入模式
/// 
/// @copyright Copyright (c) 2026
/// 
#ifndef LIBCA_EM_DRIVER_HC_SR04_H
#define LIBCA_EM_DRIVER_HC_SR04_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_HC_SR04_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_HC_SR04_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_HC_SR04_PORT_MODE
#define LIBCA_HC_SR04_PORT_MODE LIBCA_HC_SR04_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_HC_SR04_PORT_MODE == LIBCA_HC_SR04_PORT_MODE_EXTERN)
/// @brief 写引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚号
/// @param value 电平值
extern void port_hc_sr04_write_pin(void* gpio, u16 pin, u8 value);
/// @brief 读引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚号
/// @return 当前电平值
extern u8 port_hc_sr04_read_pin(void* gpio, u16 pin);
/// @brief 微秒延时
/// @param us 延时时间（微秒）
extern void port_hc_sr04_delay_us(u32 us);
/// @brief 设置定时器计数值
/// @param tim 定时器句柄
/// @param val 计数值
extern void port_hc_sr04_tim_set_counter(void* tim, u32 val);
/// @brief 启动定时器
/// @param tim 定时器句柄
extern void port_hc_sr04_tim_start(void* tim);
/// @brief 停止定时器
/// @param tim 定时器句柄
extern void port_hc_sr04_tim_stop(void* tim);
/// @brief 读取定时器计数值（单位微秒）
/// @param tim 定时器句柄
/// @return 计数值
extern u32 port_hc_sr04_tim_get_counter(void* tim);
/// @brief 互斥量等待（可选，弱符号默认为空实现）
extern void port_hc_sr04_mutex_pend(void);
/// @brief 互斥量释放（可选，弱符号默认为空实现）
extern void port_hc_sr04_mutex_post(void);

#elif (LIBCA_HC_SR04_PORT_MODE == LIBCA_HC_SR04_PORT_MODE_DYNAMIC)
typedef struct hc_sr04_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);  // GPIO写引脚
    u8   (*read_pin)(void* gpio, u16 pin);              // GPIO读引脚
    void (*delay_us)(u32 us);                           // 微秒延时
    void (*tim_set_counter)(void* tim, u32 val);        // 设置定时器计数值
    void (*tim_start)(void* tim);                       // 启动定时器
    void (*tim_stop)(void* tim);                        // 停止定时器
    u32  (*tim_get_counter)(void* tim);                 // 读取定时器计数值
    void (*mutex_pend)(void);                           // 互斥量等待（可为NULL）
    void (*mutex_post)(void);                           // 互斥量释放（可为NULL）
} hc_sr04_port_t;
void hc_sr04_bind_port(const hc_sr04_port_t* port);
bool hc_sr04_port_is_registered(void);

#else
#error "Invalid HC_SR04 port mode"
#endif

typedef struct hc_sr04 {
    void* trig_port;
    u16   trig_pin;
    void* echo_port;
    u16   echo_pin;
    void* tim;           // 计时器句柄
    double distance;     // 单位：cm
} hc_sr04_t;

/* 错误码 */
#define HC_SR04_OK                      0
#define HC_SR04_ERR_TIMEOUT             (-1)
#define HC_SR04_ERR_INVALID_PARAM       (-2)

// 初始化设备（绑定引脚与定时器）
void hc_sr04_init(hc_sr04_t* self, void* trig_port, u16 trig_pin, void* echo_port, u16 echo_pin, void* tim);
// 发起一次测距，返回 HC_SR04_OK 或 错误码，并把测到的距离写入 self->distance
i32 hc_sr04_measure(hc_sr04_t* self);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_HC_SR04_H
