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

// 状态定义
#define SC_STATE_START    0
#define SC_STATE_FINISHED ((u32)-1)

// 编译器特性检测：GCC/Clang 支持标签作为值 (Labels as Values)
#if defined(__GNUC__) || defined(__clang__)
#define SC_HAS_LABEL_SUPPORT 1
#else 
#define SC_HAS_LABEL_SUPPORT 0
#endif

typedef struct scoroutine scoroutine_t;

typedef void (*sc_task_t)(scoroutine_t *ctx);

struct scoroutine
{
    sc_task_t cfunc;
#ifdef SC_HAS_LABEL_SUPPORT
    void *state;
#else
    u32 state;
#endif
    u32 delay_tick;
};

// 辅助宏：用于生成唯一的标签名
#define SC_CONCAT_IMPL(x, y) x##y
#define SC_CONCAT(x, y)      SC_CONCAT_IMPL(x, y)

// 关闭 clang-format 以保持宏定义的紧凑性
// clang-format off

#ifdef SC_HAS_LABEL_SUPPORT

#define SC_YIELD_WITH_ID(id)                                                   \
    do {                                                                       \
        __label__ SC_CONCAT(sc_label_, id);                                    \
        _sc->state = &&SC_CONCAT(sc_label_, id);                               \
        return;                                                                \
        SC_CONCAT(sc_label_, id):;                                             \
    } while (0)

#define SC_YIELD_EXPAND(id) SC_YIELD_WITH_ID(id)

/**
 * @brief 标签模式 (GCC/Clang): 彻底解决嵌套 switch 冲突
 */
#define sc_begin(ctx)                                                          \
    scoroutine_t *_sc = (ctx);                                                 \
    if (_sc->state == (void *)SC_STATE_FINISHED) return;                       \
    if (_sc->state != NULL) goto *_sc->state;                                  \
    {

#define sc_yield() SC_YIELD_EXPAND(__COUNTER__)

#define sc_end()                                                               \
    _sc->state = (void *)SC_STATE_FINISHED;                                    \
    }

#define sc_is_finished(ctx)                                                    \
    ((ctx)->state == (void *)SC_STATE_FINISHED)

#else

/**
 * @brief Switch 模式 (通用): 兼容性好，但内部不能嵌套 switch
 */
#define sc_begin(ctx)                                                          \
    scoroutine_t *_sc = (ctx);                                                 \
    if (_sc->state == SC_STATE_FINISHED) return;                               \
    switch (_sc->state) {                                                      \
    case SC_STATE_START:

#define sc_yield()                                                             \
    do {                                                                       \
        _sc->state = __LINE__;                                                 \
        return;                                                                \
        case __LINE__:;                                                        \
    } while (0)

#define sc_end()                                                               \
    _sc->state = SC_STATE_FINISHED;                                            \
    }

#define sc_is_finished(ctx)                                                    \
    ((ctx)->state == SC_STATE_FINISHED)

#endif

#define sc_delay_ms(ms)                                                        \
    do {                                                                       \
        _sc->delay_tick = ms;                                                  \
        sc_yield();                                                            \
    } while (0)

/**
 * @brief 等待子协程完成 (嵌套支持)
 */
#define sc_await(child_func, child_ctx)                                        \
    do {                                                                       \
        while (!sc_is_finished(child_ctx)) {                                   \
            child_func(child_ctx);                                             \
            if (!sc_is_finished(child_ctx)) {                                  \
                sc_yield();                                                    \
            }                                                                  \
        }                                                                      \
    } while (0)

// 打开 clang-format
// clang-format on

#define SC_MAX_SIZE 10
extern scoroutine_t g_scoroutines[SC_MAX_SIZE];
extern usize        g_scoroutine_count;

#define sc_create_coroutine(cfunc_name)                                        \
    do {                                                                       \
        g_scoroutines[g_scoroutine_count].cfunc = (sc_task_t)cfunc_name;       \
        g_scoroutines[g_scoroutine_count].state = 0;                           \
        g_scoroutines[g_scoroutine_count].delay_tick = 0;                      \
        g_scoroutine_count++;                                                  \
    } while (0)

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
            if (g_scoroutines[i].delay_tick == 0 && !sc_is_finished(&g_scoroutines[i])) {
                g_scoroutines[i].cfunc(&g_scoroutines[i]);
            }
        }
    }
}

#endif   // !LIBCA_SCOROUTINE_H
