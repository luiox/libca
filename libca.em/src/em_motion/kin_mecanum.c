/// @file kin_mecanum.c
/// @author canrad (1517807724@qq.com)
/// @brief 麦克纳姆底盘运动学实现
/// @version 0.2
/// @date 2026-03-21
///
/// @copyright Copyright (c) 2026
///

#define _USE_MATH_DEFINES
#include "kin_mecanum.h"

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

void mecanum_ik(f32 vx, f32 vy, f32 wz, const mecanum_param_t* p, f32 wheel_speed[4])
{
    if (!p || !wheel_speed || p->wheel_radius <= 0.0f) {
        return;
    }

    f32 r = p->wheel_radius;
    f32 k = p->half_length + p->half_width;

    wheel_speed[0] = (vx - vy - k * wz) / r;
    wheel_speed[1] = (vx + vy + k * wz) / r;
    wheel_speed[2] = (vx + vy - k * wz) / r;
    wheel_speed[3] = (vx - vy + k * wz) / r;
}

void mecanum_fk(const f32 wheel_speed[4], const mecanum_param_t* p, f32* vx, f32* vy, f32* wz)
{
    if (!wheel_speed || !p || p->wheel_radius <= 0.0f) {
        if (vx)
            *vx = 0.0f;
        if (vy)
            *vy = 0.0f;
        if (wz)
            *wz = 0.0f;
        return;
    }

    f32 r   = p->wheel_radius;
    f32 k   = p->half_length + p->half_width;
    f32 wFL = wheel_speed[0];
    f32 wFR = wheel_speed[1];
    f32 wRL = wheel_speed[2];
    f32 wRR = wheel_speed[3];

    if (vx) {
        *vx = r * (wFL + wFR + wRL + wRR) / 4.0f;
    }
    if (vy) {
        *vy = r * (-wFL + wFR + wRL - wRR) / 4.0f;
    }
    if (wz) {
        if (k > 0.0f) {
            *wz = r * (-wFL + wFR - wRL + wRR) / (4.0f * k);
        }
        else {
            *wz = 0.0f;
        }
    }
}

void mecanum_odometry_init(mecanum_odometry_t* odo)
{
    if (!odo) {
        return;
    }
    odo->x     = 0.0f;
    odo->y     = 0.0f;
    odo->theta = 0.0f;
}

void mecanum_odometry_update(mecanum_odometry_t* odo, f32 vx, f32 vy, f32 wz, f32 dt)
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