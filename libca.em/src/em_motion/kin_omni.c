/// @file kin_omni.c
/// @author canrad (1517807724@qq.com)
/// @brief 全向轮（3轮）运动学实现
/// @version 0.2
/// @date 2026-03-21
///
/// @copyright Copyright (c) 2026
///

#define _USE_MATH_DEFINES
#include "kin_omni.h"

#include <math.h>

static void normalize_angle(f32* angle)
{
    while (*angle > (f32)M_PI) {
        *angle -= 2.0f * (f32)M_PI;
    }
    while (*angle < -(f32)M_PI) {
        *angle += 2.0f * (f32)M_PI;
    }
}

void omni3_ik(f32 vx, f32 vy, f32 wz, const omni3_param_t* p, f32 wheel_speed[3])
{
    const f32 sqrt3 = 1.7320508f;

    if (!p || !wheel_speed || p->wheel_radius <= 0.0f) {
        return;
    }

    f32 r = p->wheel_radius;
    f32 L = p->chassis_radius;

    wheel_speed[0] = (-0.5f * vx + (sqrt3 * 0.5f) * vy + L * wz) / r;
    wheel_speed[1] = (-0.5f * vx - (sqrt3 * 0.5f) * vy + L * wz) / r;
    wheel_speed[2] = (vx + L * wz) / r;
}

void omni3_fk(const f32 wheel_speed[3], const omni3_param_t* p, f32* vx, f32* vy, f32* wz)
{
    const f32 sqrt3 = 1.7320508f;

    if (!wheel_speed || !p || p->wheel_radius <= 0.0f) {
        if (vx)
            *vx = 0.0f;
        if (vy)
            *vy = 0.0f;
        if (wz)
            *wz = 0.0f;
        return;
    }

    f32 r  = p->wheel_radius;
    f32 L  = p->chassis_radius;
    f32 w0 = wheel_speed[0];
    f32 w1 = wheel_speed[1];
    f32 w2 = wheel_speed[2];

    if (vx) {
        *vx = r * (-w0 - w1 + 2.0f * w2) / 3.0f;
    }
    if (vy) {
        *vy = r * (sqrt3 * w0 - sqrt3 * w1) / 3.0f;
    }
    if (wz) {
        if (L > 0.0f) {
            *wz = r * (w0 + w1 + w2) / (3.0f * L);
        }
        else {
            *wz = 0.0f;
        }
    }
}

void omni3_odometry_init(omni3_odometry_t* odo)
{
    if (!odo) {
        return;
    }
    odo->x     = 0.0f;
    odo->y     = 0.0f;
    odo->theta = 0.0f;
}

void omni3_odometry_update(omni3_odometry_t* odo, f32 vx, f32 vy, f32 wz, f32 dt)
{
    if (!odo || dt <= 0.0f) {
        return;
    }

    f32 cos_theta = cosf(odo->theta);
    f32 sin_theta = sinf(odo->theta);

    f32 vx_global = vx * cos_theta - vy * sin_theta;
    f32 vy_global = vx * sin_theta + vy * cos_theta;

    odo->x += vx_global * dt;
    odo->y += vy_global * dt;
    odo->theta += wz * dt;

    normalize_angle(&odo->theta);
}