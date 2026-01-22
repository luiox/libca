/**
 * @file hc_sr04.h
 * @author canrad (1517807724@qq.com)
 * @brief HC-SR04 超声波测距驱动
 * @version 0.1
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_HC_SR04_H
#define LIBCA_EM_DRIVER_HC_SR04_H

#include "../em_base/datatype.h"

typedef struct hc_sr04_port {
    /* GPIO 操作 */
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8   (*read_pin)(void* gpio, u16 pin);
    /* 微秒级延时 */
    void (*delay_us)(u32 us);
    /* 定时器操作 （计数单位应为微秒） */
    void (*tim_set_counter)(void* tim, u32 val);
    void (*tim_start)(void* tim);
    void (*tim_stop)(void* tim);
    u32  (*tim_get_counter)(void* tim);
    /* 可选互斥回调（可为 NULL） */
    void (*mutex_pend)(void);
    void (*mutex_post)(void);
} hc_sr04_port_t;

void hc_sr04_bind_port(const hc_sr04_port_t* port);
bool hc_sr04_port_is_registered(void);

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
#define HC_SR04_ERR_PORT_NOT_REGISTERED (-1)
#define HC_SR04_ERR_TIMEOUT             (-2)
#define HC_SR04_ERR_INVALID_PARAM       (-3)

// 初始化设备（绑定引脚与定时器）
void hc_sr04_init(hc_sr04_t* self, void* trig_port, u16 trig_pin, void* echo_port, u16 echo_pin, void* tim);
// 发起一次测距，返回 HC_SR04_OK 或 错误码，并把测到的距离写入 self->distance
i32 hc_sr04_measure(hc_sr04_t* self);

#endif // LIBCA_EM_DRIVER_HC_SR04_H
