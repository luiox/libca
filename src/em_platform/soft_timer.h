/**
 * @file soft_timer.h
 * @author canrad (1517807724@qq.com)
 * @brief 嵌入式时间管理与软件定时器基础组件
 * @version 0.1
 * @date 2025-12-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef LIBCA_EM_SOFT_TIMER_H
#define LIBCA_EM_SOFT_TIMER_H

#include "datatype.h"

// 时间戳类型，单位微秒，开机后，一般来说基于SysTick或其他时基
#if HAS_INT64
typedef u64 timestamp_t;
#else
// 如果没有64位，使用32位时间戳
typedef u32 timestamp_t;
#endif

// 时间提供者函数指针类型 (例如指向 HAL_GetTick)
typedef timestamp_t (*time_get_fn_t)(void);

// 要么选择设置ms的时间提供器
void time_set_ms_provider(time_get_fn_t provider);
// 要么选择调用下面这个更新函数来手动更新时间
// @param ms_delta 距离上次调用过去的毫秒数
void time_update_tick_ms(timestamp_t ms_delta);

// 设置us的时间提供器
void time_set_us_provider(time_get_fn_t provider);

// 获得当前ms时间戳
timestamp_t time_get_ms(void);
// 获得当前us时间戳
timestamp_t time_get_us(void);

// 软件定时器
typedef struct soft_timer {
    timestamp_t start; // 计时开始时间
    u32 interval; // 超时周期
} soft_timer_t;

// 设置软件定时器的超时时间
static inline void soft_timer_set(soft_timer_t* t, timestamp_t interval) {
    t->start = time_get_ms(); // 默认使用 ms
    t->interval = interval;
}

// 开启软件定时器
static inline void soft_timer_start(soft_timer_t* t) {
    t->start = time_get_ms(); // 默认使用 ms
}

// 检查软件定时器是否超时
static inline bool soft_timer_is_timeout(soft_timer_t* t) {
    return (time_get_ms() - t->start) >= t->interval;
}

#endif // LIBCA_EM_SOFT_TIMER_H
