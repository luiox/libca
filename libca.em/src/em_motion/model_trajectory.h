/// @file model_trajectory.h
/// @author Canrad
/// @brief PTZ/执行器通用轨迹模型封装
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_MOTION_MODEL_TRAJECTORY_H
#define LIBCA_EM_MOTION_MODEL_TRAJECTORY_H

#include <em_base/datatype.h>

#include "trapezoidal.h"
#include "s_curve.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief 轨迹类型
typedef enum trajectory_type_e
{
    TRAJECTORY_TYPE_TRAPEZOIDAL = 0,
    TRAJECTORY_TYPE_S_CURVE     = 1,
} trajectory_type_t;

/// @brief 通用轨迹对象
typedef struct model_trajectory_s
{
    trajectory_type_t type;
    trapezoidal_t     trap;
    s_curve_t         s_curve;
} model_trajectory_t;

/// @brief 初始化梯形轨迹
void model_trajectory_init_trapezoidal(model_trajectory_t* traj, f32 start, f32 end, f32 max_vel,
                                       f32 accel);

void model_trajectory_init_s_curve(model_trajectory_t* traj, const s_curve_config_t* cfg);

/// @brief 查询轨迹目标位置
f32 model_trajectory_eval(const model_trajectory_t* traj, f32 t);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_MOTION_MODEL_TRAJECTORY_H
