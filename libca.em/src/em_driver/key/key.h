/// @file key.h
/// @author Canrad
/// @brief 硬件按键的常用代码，包括按键去抖动，按键长按、短按、双击检测等
/// @version 0.1
/// @date 2025-04-12
/// @update 0.2 添加extern外部依赖注入模式
/// update 2026-01-11 重新以OOP方式实现
///
/// @copyright Copyright (c) 2025
///
#ifndef LIBCA_EM_DRIVER_KEY_H
#define LIBCA_EM_DRIVER_KEY_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_KEY_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_KEY_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_KEY_PORT_MODE
#    define LIBCA_KEY_PORT_MODE LIBCA_KEY_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_KEY_PORT_MODE == LIBCA_KEY_PORT_MODE_EXTERN)
/// @brief 读取按键引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚编号
/// @return 电平值
extern u8 port_key_read_pin(void* gpio, u16 pin);

#elif (LIBCA_KEY_PORT_MODE == LIBCA_KEY_PORT_MODE_DYNAMIC)
typedef struct key_port
{
    u8 (*read_pin)(void* gpio, u16 pin);   // 读取引脚电平
} key_port_t;
void key_bind_port(const key_port_t* port);
bool key_port_is_registered(void);

#else
#    error "Invalid KEY port mode"
#endif

#define KEY_STATE_PRESS 1
#define KEY_STATE_RELEASE 0

typedef struct key
{
    void* gpio;
    u16   pin;

    // 按键状态，KEY_STATE_PRESS或者是KEY_STATE_RELEASE
    u8 key_state;
    // 状态机，不需要外部手工操作
    u8 judge_state;
    // 短按标志位
    u8 short_flag;
    // 正常按键标志位
    u8 normal_flag;
    // 长按标志位
    u8 long_flag;
    // 时间，tick为单位
    // 如果是确定按下的时候，是按下的时间，要不然是第一次按下以后，滤波的时间
    u16 time;
} key_t;

void key_init(key_t* self);
// 扫描按键，更新按键状态，一般来说放在10ms或者20ms这种中断里面
void key_scan_all(key_t* keys, usize keys_size);

///////////////////////////////////////////////////////////////////////////////
// 配置信息

// 根据实际按键个数的情况，定义这个宏，默认是4个按键
#define REAL_KEYS_SIZE 4
// 按键消抖时间，一般是1tick，也就是1ms
#define KEY_ELIMIT_DITCHING_TICK 1
// 按键短按最大时间，是一个阈值，如果是10ms扫描的情况下，默认100tick，
// 此时1tick是10ms，也就是说1000ms以内的按下都算是短按
#define KEY_MAX_SHORT_TICK 100
// 按键长按最小时间，是一个阈值，如果是10ms扫描的情况下，默认200tick，
// 此时1tick是10ms，也就是说2000ms以上的按下才算是长按
#define KEY_MIN_LONG_TICK 200

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_KEY_H
