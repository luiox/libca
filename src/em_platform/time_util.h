// 提供时间相关的工具函数
#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include "../em_base/datatype.h"   
#include <stdbool.h>

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

// 阻塞延时函数
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

// 非阻塞延时函数（单位：毫秒）
void delay_us_noblock(uint32_t us);
void delay_ms_noblock(uint32_t ms);

void ms_timer_irq_handler(void);

// 这个时间差不多在49天以后溢出
u32 time_get_current_tick(void);

// 获取当前时间戳（单位：毫秒）
//uint32_t get_current_timestamp();

// 是否过去了特定时间
//bool passed_ms(uint32_t timestamp);

// 初始化时间戳（通常在系统启动时调用一次）
//void init_timestamp();

#endif // TIMEUTIL_H
