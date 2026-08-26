/// @file kin_ackermann.c
/// @author Canrad
/// @brief 阿克曼模型运动学基础实现
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///

#define _USE_MATH_DEFINES
#include "kin_ackermann.h"

#include <math.h>

f32 ackermann_calc_steer_angle(f32 v, f32 w, f32 wheel_base)
{
    if (wheel_base <= 0.0f || w == 0.0f) {
        return 0.0f;
    }
    if (v == 0.0f) {
        return 0.0f;
    }

    return atanf((wheel_base * w) / v);
}

void ackermann_split_steer(f32 steer_center, const ackermann_param_t* p, f32* steer_left,
                           f32* steer_right)
{
    f32 tan_delta;
    f32 radius;
    f32 left_radius;
    f32 right_radius;

    if (!p || !steer_left || !steer_right || p->wheel_base <= 0.0f) {
        return;
    }

    tan_delta = tanf(steer_center);
    if (tan_delta == 0.0f || p->wheel_track <= 0.0f) {
        *steer_left  = steer_center;
        *steer_right = steer_center;
        return;
    }

    radius       = p->wheel_base / tan_delta;
    left_radius  = radius - (p->wheel_track * 0.5f);
    right_radius = radius + (p->wheel_track * 0.5f);

    if (fabsf(left_radius) < 1e-6f) {
        *steer_left = (left_radius >= 0.0f) ? (f32)M_PI_2 : -(f32)M_PI_2;
    }
    else {
        *steer_left = atanf(p->wheel_base / left_radius);
    }

    if (fabsf(right_radius) < 1e-6f) {
        *steer_right = (right_radius >= 0.0f) ? (f32)M_PI_2 : -(f32)M_PI_2;
    }
    else {
        *steer_right = atanf(p->wheel_base / right_radius);
    }
}
