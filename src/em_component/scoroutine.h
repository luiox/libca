/**
 * @file scoroutine.h
 * @author canrad (1517807724@qq.com)
 * @brief 一个基于状态机实现的无栈协程框架
 * scoroutine即simple coroutine，简单协程，基于状态机实现，无栈，无锁，无内存分配
 * @version 0.1
 * @date 2025-08-07
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef LIBCA_SCOROUTINE_H
#define LIBCA_SCOROUTINE_H

#include "../em_base/datatype.h"

typedef struct
{
    void (*cfunc)(u8 _cid);
    u32 state;
    u32 delay_tick;
} scoroutine_t;

// 关闭clangformat
// clang-format off
// 定义协程的宏
#define sc_begin(cfunc_name) void cfunc_name(u8 _cid) {\
    extern scoroutine_t g_scoroutines[];\
    scoroutine_t *_sc = &g_scoroutines[_cid];\
    switch(_sc->state) { case 0:
#define sc_yield() do { _sc->state = __LINE__; return; case __LINE__:; } while (0)
#define sc_delay_ms(ms) do { _sc->delay_tick = ms; sc_yield(); } while (0)
#define sc_end() }}
// 打开clang-format
// clang-format on

#define SC_MAX_SIZE 10
extern scoroutine_t g_scoroutines[SC_MAX_SIZE];
extern usize        g_scoroutine_count;

#define sc_create_coroutine(cfunc_name) g_scoroutines[g_scoroutine_count++].cfunc = &cfunc_name;

static void tim_1ms_handler(void)
{
    for (u8 i = 0; i < g_scoroutine_count; i++) {
        if (g_scoroutines[i].delay_tick > 0) {
            g_scoroutines[i].delay_tick--;
        }
    }
}

static void sc_scheduler_start(void)
{
    while (1) {
        for (u8 i = 0; i < g_scoroutine_count; i++) {
            if (g_scoroutines[i].delay_tick == 0) {
                g_scoroutines[i].cfunc(i);
            }
        }
    }
}

#endif   // !LIBCA_SCOROUTINE_H
