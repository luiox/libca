/**
 * @file inverted_pendulum.h
 * @author canrad (1517807724@qq.com)
 * @brief 倒立摆控制与重力补偿接口
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_MOTION_INVERTED_PENDULUM_H
#define LIBCA_EM_MOTION_INVERTED_PENDULUM_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 倒立摆控制参数
 */
typedef struct inverted_pendulum_cfg_s {
	f32 kp;
	f32 kd;
	f32 mass;
	f32 length;
	f32 gravity;
} inverted_pendulum_cfg_t;

/**
 * @brief 计算 PD 控制力矩
 *
 * @param angle 当前摆角，单位 rad
 * @param ang_vel 当前角速度，单位 rad/s
 * @param target_angle 目标摆角，单位 rad
 * @param kp 比例系数
 * @param kd 微分系数
 * @return f32 控制力矩
 */
f32 inverted_pendulum_pd(f32 angle, f32 ang_vel, f32 target_angle, f32 kp, f32 kd);

/**
 * @brief 计算重力补偿项
 *
 * @param angle 当前摆角，单位 rad
 * @param mass 摆体质量，单位 kg
 * @param length 质心到转轴距离，单位 m
 * @param gravity 重力加速度，单位 m/s^2
 * @return f32 重力补偿力矩
 */
f32 inverted_pendulum_gravity_comp(f32 angle, f32 mass, f32 length, f32 gravity);

/**
 * @brief 计算总控制力矩（PD + 重力补偿）
 *
 * @param angle 当前摆角，单位 rad
 * @param ang_vel 当前角速度，单位 rad/s
 * @param target_angle 目标摆角，单位 rad
 * @param cfg 控制参数
 * @return f32 控制力矩
 */
f32 inverted_pendulum_control(f32 angle, f32 ang_vel, f32 target_angle, const inverted_pendulum_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_INVERTED_PENDULUM_H
