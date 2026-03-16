/**
 * @file kin_mecanum.h
 * @author canrad (1517807724@qq.com)
 * @brief 麦克纳姆底盘运动学接口
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_MOTION_KIN_MECANUM_H
#define LIBCA_EM_MOTION_KIN_MECANUM_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 麦克纳姆底盘几何参数
 */
typedef struct mecanum_param_s {
    f32 wheel_radius;
    f32 half_length;
    f32 half_width;
} mecanum_param_t;

/**
 * @brief 麦克纳姆逆运动学
 *
 * @param vx x 方向线速度，单位 m/s
 * @param vy y 方向线速度，单位 m/s
 * @param wz 角速度，单位 rad/s
 * @param p 运动学参数
 * @param wheel_speed 输出轮速数组，顺序 FL/FR/RL/RR，单位 rad/s
 */
void mecanum_ik(f32 vx, f32 vy, f32 wz, const mecanum_param_t* p, f32 wheel_speed[4]);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_KIN_MECANUM_H
