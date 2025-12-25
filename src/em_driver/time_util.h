// 提供时间相关的工具函数
#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include "../em_base/datatype.h"   
#include <stdbool.h>

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
