/// @file trapezoidal.c
/// @author Canrad
/// @brief 梯形速度轨迹规划器基础实现
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///

#include "trapezoidal.h"

#include <math.h>

static f32 clamp_time(f32 t)
{
    if (t < 0.0f) {
        return 0.0f;
    }
    return t;
}

void trapezoidal_init(trapezoidal_t* plan, f32 start, f32 end, f32 max_vel, f32 accel)
{
    f32 d_acc = 0.0f;

    if (!plan) {
        return;
    }

    if (max_vel < 0.0f) {
        max_vel = -max_vel;
    }
    if (accel < 0.0f) {
        accel = -accel;
    }

    plan->cfg.start   = start;
    plan->cfg.end     = end;
    plan->cfg.max_vel = max_vel;
    plan->cfg.accel   = accel;
    plan->distance    = end - start;
    plan->direction   = (plan->distance >= 0.0f) ? 1.0f : -1.0f;
    if (plan->distance < 0.0f) {
        plan->distance = -plan->distance;
    }

    if (max_vel <= 0.0f || accel <= 0.0f || plan->distance <= 0.0f) {
        plan->t_acc      = 0.0f;
        plan->t_cruise   = 0.0f;
        plan->t_dec      = 0.0f;
        plan->t_total    = 0.0f;
        plan->triangular = true;
        return;
    }

    plan->t_acc = max_vel / accel;
    d_acc       = 0.5f * accel * plan->t_acc * plan->t_acc;

    if ((2.0f * d_acc) >= plan->distance) {
        f32 t_peak       = sqrtf(plan->distance / accel);
        plan->triangular = true;
        plan->t_acc      = t_peak;
        plan->t_cruise   = 0.0f;
        plan->t_dec      = t_peak;
    }
    else {
        plan->triangular = false;
        plan->t_cruise   = (plan->distance - 2.0f * d_acc) / max_vel;
        plan->t_dec      = plan->t_acc;
    }

    plan->t_total = plan->t_acc + plan->t_cruise + plan->t_dec;
}

f32 trapezoidal_update(const trapezoidal_t* plan, f32 time)
{
    f32 t = clamp_time(time);
    f32 a;
    f32 vmax;
    f32 s;

    if (!plan) {
        return 0.0f;
    }

    a    = plan->cfg.accel;
    vmax = plan->cfg.max_vel;

    if (plan->t_total <= 0.0f) {
        return plan->cfg.end;
    }

    if (t >= plan->t_total) {
        return plan->cfg.end;
    }

    if (t <= plan->t_acc) {
        s = 0.5f * a * t * t;
    }
    else if (t <= (plan->t_acc + plan->t_cruise)) {
        f32 s_acc  = 0.5f * a * plan->t_acc * plan->t_acc;
        f32 t_mid  = t - plan->t_acc;
        f32 v_peak = a * plan->t_acc;
        if (!plan->triangular) {
            v_peak = vmax;
        }
        s = s_acc + v_peak * t_mid;
    }
    else {
        f32 t_dec  = t - plan->t_acc - plan->t_cruise;
        f32 v_peak = a * plan->t_acc;
        f32 s_acc  = 0.5f * a * plan->t_acc * plan->t_acc;
        f32 s_mid  = 0.0f;
        if (!plan->triangular) {
            v_peak = vmax;
            s_mid  = vmax * plan->t_cruise;
        }
        s = s_acc + s_mid + v_peak * t_dec - 0.5f * a * t_dec * t_dec;
    }

    return plan->cfg.start + plan->direction * s;
}

f32 trapezoidal_calc(f32 start, f32 end, f32 max_vel, f32 accel, f32 t)
{
    trapezoidal_t plan;
    trapezoidal_init(&plan, start, end, max_vel, accel);
    return trapezoidal_update(&plan, t);
}
