/**
 * @file kin_mecanum.c
 * @author canrad (1517807724@qq.com)
 * @brief 麦克纳姆底盘运动学基础实现
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "kin_mecanum.h"

void mecanum_ik(f32 vx, f32 vy, f32 wz, const mecanum_param_t* p, f32 wheel_speed[4])
{
    f32 k;

    if (!p || !wheel_speed || p->wheel_radius <= 0.0f) {
        return;
    }

    k = p->half_length + p->half_width;

    wheel_speed[0] = (vx - vy - k * wz) / p->wheel_radius;
    wheel_speed[1] = (vx + vy + k * wz) / p->wheel_radius;
    wheel_speed[2] = (vx + vy - k * wz) / p->wheel_radius;
    wheel_speed[3] = (vx - vy + k * wz) / p->wheel_radius;
}
