/**
 * @file kin_ackermann.h
 * @author canrad (1517807724@qq.com)
 * @brief 阿克曼模型运动学接口
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_MOTION_KIN_ACKERMANN_H
#define LIBCA_EM_MOTION_KIN_ACKERMANN_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 阿克曼底盘参数
 */
typedef struct ackermann_param_s {
    f32 wheel_base;
    f32 wheel_track;
} ackermann_param_t;

/**
 * @brief 由线速度和角速度计算前轮等效转角
 *
 * @param v 线速度，单位 m/s
 * @param w 角速度，单位 rad/s
 * @param wheel_base 轴距，单位 m
 * @return f32 转向角，单位 rad
 */
f32 ackermann_calc_steer_angle(f32 v, f32 w, f32 wheel_base);

/**
 * @brief 计算左右前轮转角（简化几何）
 *
 * @param steer_center 车辆中心等效转角，单位 rad
 * @param p 阿克曼参数
 * @param steer_left 左前轮转角输出，单位 rad
 * @param steer_right 右前轮转角输出，单位 rad
 */
void ackermann_split_steer(
    f32 steer_center,
    const ackermann_param_t* p,
    f32* steer_left,
    f32* steer_right);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_KIN_ACKERMANN_H
