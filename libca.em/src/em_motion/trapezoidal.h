/// @file trapezoidal.h
/// @author Canrad
/// @brief 梯形速度轨迹规划器
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_MOTION_TRAPEZOIDAL_H
#define LIBCA_EM_MOTION_TRAPEZOIDAL_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief 梯形规划参数
typedef struct trapezoidal_config_s {
	f32 start;
	f32 end;
	f32 max_vel;
	f32 accel;
} trapezoidal_config_t;

/// @brief 梯形规划状态
typedef struct trapezoidal_s {
	trapezoidal_config_t cfg;
	f32 distance;
	f32 direction;
	f32 t_acc;
	f32 t_cruise;
	f32 t_dec;
	f32 t_total;
	bool triangular;
} trapezoidal_t;

/// @brief 初始化梯形规划器
///
/// @param plan 规划器状态对象
/// @param start 起点位置
/// @param end 终点位置
/// @param max_vel 最大速度（绝对值）
/// @param accel 最大加速度（绝对值）
void trapezoidal_init(trapezoidal_t* plan, f32 start, f32 end, f32 max_vel, f32 accel);

/// @brief 查询给定时刻的位置目标
///
/// @param plan 规划器状态对象
/// @param time 相对起始时刻，单位 s
/// @return f32 目标位置
f32 trapezoidal_update(const trapezoidal_t* plan, f32 time);

/// @brief 无状态一次性计算接口
///
/// @param start 起点位置
/// @param end 终点位置
/// @param max_vel 最大速度（绝对值）
/// @param accel 最大加速度（绝对值）
/// @param t 相对起始时刻，单位 s
/// @return f32 目标位置
f32 trapezoidal_calc(f32 start, f32 end, f32 max_vel, f32 accel, f32 t);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_TRAPEZOIDAL_H
