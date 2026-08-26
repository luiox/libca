/// @file kin_diff.h
/// @author Canrad
/// @brief 差速底盘运动学接口
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_MOTION_KIN_DIFF_H
#define LIBCA_EM_MOTION_KIN_DIFF_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief 二维位姿（世界坐标系）
typedef struct pose2d_s {
	f32 x;
	f32 y;
	f32 theta;
} pose2d_t;

/// @brief 差速逆运动学
/// 输入期望线速度和角速度，输出左右轮线速度。
///
/// @param v 线速度，单位 m/s
/// @param w 角速度，单位 rad/s
/// @param wheel_base 轮距，单位 m
/// @param left 左轮线速度输出，单位 m/s
/// @param right 右轮线速度输出，单位 m/s
void diff_drive_ik(f32 v, f32 w, f32 wheel_base, f32* left, f32* right);

/// @brief 差速正运动学
/// 输入左右轮线速度，输出底盘线速度与角速度。
///
/// @param left 左轮线速度，单位 m/s
/// @param right 右轮线速度，单位 m/s
/// @param wheel_base 轮距，单位 m
/// @param v 线速度输出，单位 m/s
/// @param w 角速度输出，单位 rad/s
void diff_drive_fk(f32 left, f32 right, f32 wheel_base, f32* v, f32* w);

/// @brief 差速里程计积分
///
/// @param left 左轮线速度，单位 m/s
/// @param right 右轮线速度，单位 m/s
/// @param wheel_base 轮距，单位 m
/// @param dt 时间步长，单位 s
/// @param pose 位姿输入输出
void diff_drive_odometry(f32 left, f32 right, f32 wheel_base, f32 dt, pose2d_t* pose);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_KIN_DIFF_H
