/// @file inverted_pendulum.c
/// @author Canrad
/// @brief 倒立摆控制与重力补偿基础实现
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///

#include "inverted_pendulum.h"

#include <math.h>

f32 inverted_pendulum_pd(f32 angle, f32 ang_vel, f32 target_angle, f32 kp, f32 kd)
{
    return kp * (target_angle - angle) - kd * ang_vel;
}

f32 inverted_pendulum_gravity_comp(f32 angle, f32 mass, f32 length, f32 gravity)
{
    return mass * gravity * length * sinf(angle);
}

f32 inverted_pendulum_control(f32 angle, f32 ang_vel, f32 target_angle,
                              const inverted_pendulum_cfg_t* cfg)
{
    if (!cfg) {
        return 0.0f;
    }

    return inverted_pendulum_pd(angle, ang_vel, target_angle, cfg->kp, cfg->kd) +
           inverted_pendulum_gravity_comp(angle, cfg->mass, cfg->length, cfg->gravity);
}
