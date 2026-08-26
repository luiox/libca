/// @file s_curve.c
/// @author Canrad
/// @brief S 曲线轨迹规划实现
/// @version 0.2
/// @date 2026-03-21
///
/// @copyright Copyright (c) 2026
///

#include "s_curve.h"
#include <math.h>

static void s_curve_eval_internal(const s_curve_t* plan, f32 t, f32* out_pos, f32* out_vel,
                                  f32* out_acc)
{
    if (!plan) {
        if (out_pos)
            *out_pos = 0.0f;
        if (out_vel)
            *out_vel = 0.0f;
        if (out_acc)
            *out_acc = 0.0f;
        return;
    }

    if (t <= 0.0f) {
        if (out_pos)
            *out_pos = plan->cfg.start;
        if (out_vel)
            *out_vel = 0.0f;
        if (out_acc)
            *out_acc = 0.0f;
        return;
    }
    if (t >= plan->total_time) {
        if (out_pos)
            *out_pos = plan->cfg.end;
        if (out_vel)
            *out_vel = 0.0f;
        if (out_acc)
            *out_acc = 0.0f;
        return;
    }

    f32 sign  = (plan->cfg.end >= plan->cfg.start) ? 1.0f : -1.0f;
    f32 j     = plan->cfg.max_jerk * sign;
    f32 pos   = plan->cfg.start;
    f32 vel   = 0.0f;
    f32 acc   = 0.0f;
    f32 t_seg = t;

    f32 T[S_CURVE_SEG_COUNT];
    for (int i = 0; i < S_CURVE_SEG_COUNT; i++) {
        T[i] = plan->T[i];
    }

    if (t_seg <= T[0]) {
        f32 dt = t_seg;
        acc    = j * dt;
        vel    = 0.5f * j * dt * dt;
        pos    = pos + (1.0f / 6.0f) * j * dt * dt * dt;
    }
    else {
        t_seg -= T[0];
        pos += (1.0f / 6.0f) * j * T[0] * T[0] * T[0];
        vel = 0.5f * j * T[0] * T[0];
        acc = j * T[0];

        if (t_seg <= T[1]) {
            f32 dt = t_seg;
            pos    = pos + vel * dt + 0.5f * acc * dt * dt;
            vel    = vel + acc * dt;
        }
        else {
            t_seg -= T[1];
            pos = pos + vel * T[1] + 0.5f * acc * T[1] * T[1];
            vel = vel + acc * T[1];

            if (t_seg <= T[2]) {
                f32 dt      = t_seg;
                f32 acc_end = acc - j * dt;
                pos = pos + vel * dt + 0.5f * acc * dt * dt - (1.0f / 6.0f) * j * dt * dt * dt;
                vel = vel + acc * dt - 0.5f * j * dt * dt;
                acc = acc_end;
            }
            else {
                t_seg -= T[2];
                pos = pos + vel * T[2] + 0.5f * acc * T[2] * T[2] -
                      (1.0f / 6.0f) * j * T[2] * T[2] * T[2];
                vel = plan->v_limit;
                acc = 0.0f;

                if (t_seg <= T[3]) {
                    pos = pos + vel * t_seg;
                }
                else {
                    t_seg -= T[3];
                    pos = pos + vel * T[3];

                    if (t_seg <= T[4]) {
                        f32 dt = t_seg;
                        acc    = -j * dt;
                        pos    = pos + vel * dt + 0.5f * acc * dt * dt;
                        vel    = vel + acc * dt;
                    }
                    else {
                        t_seg -= T[4];
                        acc = -plan->a_limit;
                        pos = pos + vel * T[4] + 0.5f * acc * T[4] * T[4];
                        vel = vel + acc * T[4];

                        if (t_seg <= T[5]) {
                            f32 dt = t_seg;
                            pos    = pos + vel * dt + 0.5f * acc * dt * dt;
                            vel    = vel + acc * dt;
                        }
                        else {
                            t_seg -= T[5];
                            pos = pos + vel * T[5] + 0.5f * acc * T[5] * T[5];
                            vel = vel + acc * T[5];

                            if (t_seg <= T[6]) {
                                f32 dt = t_seg;
                                pos    = pos + vel * dt + 0.5f * acc * dt * dt +
                                      (1.0f / 6.0f) * j * dt * dt * dt;
                                vel = vel + acc * dt + 0.5f * j * dt * dt;
                                acc = acc + j * dt;
                            }
                        }
                    }
                }
            }
        }
    }

    if (out_pos)
        *out_pos = pos;
    if (out_vel)
        *out_vel = vel;
    if (out_acc)
        *out_acc = acc;
}

void s_curve_init(s_curve_t* plan, const s_curve_config_t* cfg)
{
    if (!plan || !cfg) {
        return;
    }

    plan->cfg = *cfg;

    for (int i = 0; i < S_CURVE_SEG_COUNT; i++) {
        plan->T[i] = 0.0f;
    }
    plan->total_time = 0.0f;
    plan->a_limit    = cfg->max_acc;
    plan->v_limit    = cfg->max_vel;

    f32 d     = cfg->end - cfg->start;
    f32 d_abs = fabsf(d);
    f32 sign  = (d >= 0.0f) ? 1.0f : -1.0f;

    f32 v_max = cfg->max_vel;
    f32 a_max = cfg->max_acc;
    f32 j_max = cfg->max_jerk;

    f32 t_j       = a_max / j_max;
    f32 v_reach_j = 0.5f * j_max * t_j * t_j;
    f32 d_reach_j = j_max * t_j * t_j * t_j / 6.0f;

    if (v_reach_j > v_max) {
        t_j       = sqrtf(v_max / j_max);
        v_reach_j = 0.5f * j_max * t_j * t_j;
        d_reach_j = j_max * t_j * t_j * t_j / 6.0f;
        a_max     = j_max * t_j;
    }

    f32 v_const_possible = v_max - v_reach_j;
    f32 d_const          = v_const_possible * v_const_possible / a_max;
    f32 d_accel =
        v_reach_j * t_j + d_reach_j + 0.5f * (v_max + v_reach_j) * (v_max - v_reach_j) / a_max;

    f32 d_total_min = 2.0f * d_accel;

    if (d_abs < d_total_min) {
        f32 t_a = cbrtf(4.0f * d_abs / j_max);
        t_j     = 0.5f * t_a;

        plan->T[S_CURVE_SEG_ACCEL_JERK_UP]   = t_j;
        plan->T[S_CURVE_SEG_ACCEL_CONST]     = 0.0f;
        plan->T[S_CURVE_SEG_ACCEL_JERK_DOWN] = t_j;
        plan->T[S_CURVE_SEG_VEL_CONST]       = 0.0f;
        plan->T[S_CURVE_SEG_DECEL_JERK_UP]   = t_j;
        plan->T[S_CURVE_SEG_DECEL_CONST]     = 0.0f;
        plan->T[S_CURVE_SEG_DECEL_JERK_DOWN] = t_j;

        plan->v_limit = j_max * t_j * t_j / 2.0f;
        plan->a_limit = j_max * t_j;
    }
    else {
        f32 d_remaining = d_abs - 2.0f * d_accel;
        f32 t_const     = d_remaining / v_max;

        plan->T[S_CURVE_SEG_ACCEL_JERK_UP]   = t_j;
        plan->T[S_CURVE_SEG_ACCEL_CONST]     = (v_max - 2.0f * v_reach_j) / a_max;
        plan->T[S_CURVE_SEG_ACCEL_JERK_DOWN] = t_j;
        plan->T[S_CURVE_SEG_VEL_CONST]       = t_const;
        plan->T[S_CURVE_SEG_DECEL_JERK_UP]   = t_j;
        plan->T[S_CURVE_SEG_DECEL_CONST]     = plan->T[S_CURVE_SEG_ACCEL_CONST];
        plan->T[S_CURVE_SEG_DECEL_JERK_DOWN] = t_j;
    }

    plan->total_time = 0.0f;
    for (int i = 0; i < S_CURVE_SEG_COUNT; i++) {
        plan->total_time += plan->T[i];
    }

    plan->v_limit *= sign;
    plan->a_limit *= sign;
}

f32 s_curve_update(const s_curve_t* plan, f32 t)
{
    f32 pos;
    s_curve_eval_internal(plan, t, &pos, NULL, NULL);
    return pos;
}

f32 s_curve_get_velocity(const s_curve_t* plan, f32 t)
{
    f32 vel;
    s_curve_eval_internal(plan, t, NULL, &vel, NULL);
    return vel;
}

f32 s_curve_get_acceleration(const s_curve_t* plan, f32 t)
{
    f32 acc;
    s_curve_eval_internal(plan, t, NULL, NULL, &acc);
    return acc;
}