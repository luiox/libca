/**
 * @file trapezoidal.h
 * @author canrad (1517807724@qq.com)
 * @brief 规划器
 * 生成平滑的运动轨迹（位置、速度、加速度随时间的变化）
 * @version 0.1
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_MOTION_TRAPEZOIDAL_H
#define LIBCA_EM_MOTION_TRAPEZOIDAL_H

#include <em_base/datatype.h>

// 初始化一个梯形规划器（可包含状态结构体）
void trapezoidal_init(trapezoidal_t *plan, float start, float end, float max_vel, float accel);

// 根据时间更新，获取当前目标位置
float trapezoidal_update(trapezoidal_t *plan, float time);

// 或者一次性计算（无状态）
float trapezoidal_calc(float start, float end, float max_vel, float accel, float t);

#endif // !LIBCA_EM_MOTION_TRAPEZOIDAL_H
