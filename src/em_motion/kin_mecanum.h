/**
 * @file kin_mecanum.h
 * @author canrad (1517807724@qq.com)
 * @brief 麦克纳姆底盘运动学接口
 * @version 0.2
 * @date 2026-03-21
 *
 * @copyright Copyright (c) 2026
 *
 * 四轮麦克纳姆布局（俯视）：
 *   FL  FR  (前)
 *   RL  RR  (后)
 * 轮子辊子方向：FL/RR 指向右后，FR/RL 指向左后
 */

#ifndef LIBCA_EM_MOTION_KIN_MECANUM_H
#define LIBCA_EM_MOTION_KIN_MECANUM_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mecanum_param_s
{
    f32 wheel_radius;
    f32 half_length;
    f32 half_width;
} mecanum_param_t;

typedef struct mecanum_odometry_s
{
    f32 x;
    f32 y;
    f32 theta;
} mecanum_odometry_t;

void mecanum_ik(f32 vx, f32 vy, f32 wz, const mecanum_param_t* p, f32 wheel_speed[4]);

void mecanum_fk(const f32 wheel_speed[4], const mecanum_param_t* p, f32* vx, f32* vy, f32* wz);

void mecanum_odometry_init(mecanum_odometry_t* odo);

void mecanum_odometry_update(mecanum_odometry_t* odo, f32 vx, f32 vy, f32 wz, f32 dt);

#ifdef __cplusplus
}
#endif

#endif