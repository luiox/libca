// 提供时间相关的工具函数
#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include "../em_base/datatype.h"   

// // 时间戳类型，单位微秒，开机后，一般来说基于SysTick或其他时基
// #if HAS_INT64
// typedef u64 timestamp_t;
// #else
// // 如果没有64位，使用32位时间戳
// typedef u32 timestamp_t;
// #endif



// 时间提供者函数指针类型 (例如指向 HAL_GetTick)
typedef u32 (*time_get_fn_t)(void);

// 要么选择设置ms的时间提供器
void time_set_ms_provider(time_get_fn_t provider);
// 要么选择调用下面这个更新函数来手动更新时间
// @param ms_delta 距离上次调用过去的毫秒数
void time_update_tick_ms(u32 ms_delta);

// 设置us的时间提供器
void time_set_us_provider(time_get_fn_t provider);

// 获得当前ms时间戳
u32 time_get_ms(void);
// 获得当前us时间戳
u32 time_get_us(void);

// 阻塞延时函数
void delay_us(u32 us);
void delay_ms(u32 ms);

// 非阻塞延时函数（单位：毫秒）
void delay_us_noblock(u32 us);
void delay_ms_noblock(u32 ms);

void ms_timer_irq_handler(void);

// 这个时间差不多在49天以后溢出
u32 time_get_current_tick(void);

// 获取当前时间戳（单位：毫秒）
//uint32_t get_current_timestamp();

// 是否过去了特定时间
//bool passed_ms(uint32_t timestamp);

// 初始化时间戳（通常在系统启动时调用一次）
//void init_timestamp();

// 软件定时器
typedef struct soft_timer {
    u32 start; // 计时开始时间
    u32 interval; // 超时周期
} soft_timer_t;

// 设置软件定时器的超时时间
static inline void soft_timer_set(soft_timer_t* t, u32 interval) {
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


#endif // TIMEUTIL_H
