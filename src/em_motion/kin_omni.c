/**
 * @file kin_omni.c
 * @author canrad (1517807724@qq.com)
 * @brief 全向轮（3轮）运动学基础实现
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "kin_omni.h"

#include <math.h>

void omni3_ik(f32 vx, f32 vy, f32 wz, const omni3_param_t* p, f32 wheel_speed[3])
{
    const f32 sqrt3 = 1.7320508f;

    if (!p || !wheel_speed || p->wheel_radius <= 0.0f) {
        return;
    }

    wheel_speed[0] = (-0.5f * vx + (sqrt3 * 0.5f) * vy + p->chassis_radius * wz) / p->wheel_radius;
    wheel_speed[1] = (-0.5f * vx - (sqrt3 * 0.5f) * vy + p->chassis_radius * wz) / p->wheel_radius;
    wheel_speed[2] = (vx + p->chassis_radius * wz) / p->wheel_radius;
}
