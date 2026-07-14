/// @file kin_omni.h
/// @author canrad (1517807724@qq.com)
/// @brief 全向轮（3轮）运动学接口
/// @version 0.2
/// @date 2026-03-21
///
/// @copyright Copyright (c) 2026
///
/// 三轮全向轮布局（俯视）：
///       W2 (后方)
///        |
///   W0 /   \ W1
///     /_____\
///     前方
/// 轮子方向：垂直于到圆心的连线，正方向为逆时针切向

#ifndef LIBCA_EM_MOTION_KIN_OMNI_H
#define LIBCA_EM_MOTION_KIN_OMNI_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct omni3_param_s
{
    f32 wheel_radius;
    f32 chassis_radius;
} omni3_param_t;

typedef struct omni3_odometry_s
{
    f32 x;
    f32 y;
    f32 theta;
} omni3_odometry_t;

void omni3_ik(f32 vx, f32 vy, f32 wz, const omni3_param_t* p, f32 wheel_speed[3]);

void omni3_fk(const f32 wheel_speed[3], const omni3_param_t* p, f32* vx, f32* vy, f32* wz);

void omni3_odometry_init(omni3_odometry_t* odo);

void omni3_odometry_update(omni3_odometry_t* odo, f32 vx, f32 vy, f32 wz, f32 dt);

#ifdef __cplusplus
}
#endif

#endif