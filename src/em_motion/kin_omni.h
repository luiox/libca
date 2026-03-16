/**
 * @file kin_omni.h
 * @author canrad (1517807724@qq.com)
 * @brief 全向轮（3/4轮）运动学接口
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_MOTION_KIN_OMNI_H
#define LIBCA_EM_MOTION_KIN_OMNI_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 三轮全向底盘参数
 */
typedef struct omni3_param_s {
    f32 wheel_radius;
    f32 chassis_radius;
} omni3_param_t;

/**
 * @brief 三轮全向逆运动学
 *
 * @param vx x 方向线速度，单位 m/s
 * @param vy y 方向线速度，单位 m/s
 * @param wz 角速度，单位 rad/s
 * @param p 运动学参数
 * @param wheel_speed 输出轮速数组（长度 3），单位 rad/s
 */
void omni3_ik(f32 vx, f32 vy, f32 wz, const omni3_param_t* p, f32 wheel_speed[3]);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_KIN_OMNI_H
