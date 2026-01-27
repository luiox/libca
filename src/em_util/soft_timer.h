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
#ifndef LIBCA_EM_UTIL_SOFT_TIMER_H
#define LIBCA_EM_UTIL_SOFT_TIMER_H

#include "../em_base/datatype.h"

// 超时定时器
typedef struct timeout_timer{
    timestamp_t start_time; // 开始时间
    u32 timeout_ms;         // 超时时间，单位毫秒
}timeout_timer_t;

// 初始化超时定时器
static inline void timeout_timer_init(timeout_timer_t* self, timestamp_t start, u32 timeout_ms){
    self->start_time = start;
    self->timeout_ms = timeout_ms;
}

// 检查超时定时器是否超时
static inline bool timeout_timer_is_timeout(timeout_timer_t* self, timestamp_t current){
    return (current - self->start_time) >= self->timeout_ms;
}

// 重置超时定时器
static inline void timeout_timer_reset(timeout_timer_t* self, timestamp_t start){
    self->start_time = start;
}

// 累计定时器，如果不用开始时间，可以存0，这样子就是一个简单的累计计时器
typedef struct acumulate_timer{
    timestamp_t start_time; // 开始时间
    u32 elapsed_ms;       // 已经过的时间，单位毫秒
}acumulate_timer_t;

// 初始化累计定时器
static inline void acumulate_timer_init(acumulate_timer_t* self, timestamp_t start){
    self->start_time = start;
    self->elapsed_ms = 0;
}

// 重置累计定时器
static inline void acumulate_timer_reset(acumulate_timer_t* self, timestamp_t start){
    self->start_time = start;
    self->elapsed_ms = 0;
}

// 更新累计定时器
static inline void acumulate_timer_update(acumulate_timer_t* self, timestamp_t current){
    self->elapsed_ms += current;
}

// 获取累计的时间，单位毫秒
static inline u32 acumulate_timer_get_elapsed(acumulate_timer_t* self){
    return self->elapsed_ms;
}

// 是否累计超过指定时间
static inline bool acumulate_timer_is_elapsed(acumulate_timer_t* self, u32 ms){
    return self->elapsed_ms >= ms;
}

#endif // LIBCA_EM_UTIL_SOFT_TIMER_H
