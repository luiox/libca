/**
 * @file s_curve.c
 * @author canrad (1517807724@qq.com)
 * @brief S 曲线轨迹规划基础骨架
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "s_curve.h"

void s_curve_init(s_curve_t* plan, const s_curve_config_t* cfg)
{
    if (!plan || !cfg) {
        return;
    }

    plan->cfg = *cfg;
    plan->total_time = 0.0f;
}

f32 s_curve_update(const s_curve_t* plan, f32 t)
{
    unused_param(t);

    if (!plan) {
        return 0.0f;
    }

    return plan->cfg.start;
}
