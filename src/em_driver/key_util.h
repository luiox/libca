/**
 * @file key_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 硬件按键的常用代码，包括按键去抖动，按键长按、短按、双击检测等
 * @version 0.1
 * @date 2025-04-12
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef KEY_UTIL_H
#define KEY_UTIL_H

#include "../base/datatype.h" 

#define KEY_STATE_PRESS 1
#define KEY_STATE_RELEASE 0

typedef struct
{
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

// 扫描按键，更新按键状态，一般来说放在10ms或者20ms这种中断里面
void key_scan_all(void);

// 获取某个id的按键结构体，以查询数据
key_t* key_get(i8 id);

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

#endif   // !KEY_UTIL_H
