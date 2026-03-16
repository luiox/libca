/**
 * @file model_trajectory.c
 * @author canrad (1517807724@qq.com)
 * @brief PTZ/执行器通用轨迹模型封装基础实现
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "model_trajectory.h"

void model_trajectory_init_trapezoidal(
    model_trajectory_t* traj,
    f32 start,
    f32 end,
    f32 max_vel,
    f32 accel)
{
    if (!traj) {
        return;
    }

    traj->type = TRAJECTORY_TYPE_TRAPEZOIDAL;
    trapezoidal_init(&traj->trap, start, end, max_vel, accel);
}

f32 model_trajectory_eval(const model_trajectory_t* traj, f32 t)
{
    if (!traj) {
        return 0.0f;
    }

    switch (traj->type) {
        case TRAJECTORY_TYPE_TRAPEZOIDAL:
            return trapezoidal_update(&traj->trap, t);
        case TRAJECTORY_TYPE_S_CURVE:
            return trapezoidal_update(&traj->trap, t);
        default:
            return 0.0f;
    }
}
