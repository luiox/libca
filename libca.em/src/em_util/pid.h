/// @file pid.h
/// @author Canrad
/// @brief pid算法，包含位置式和增量式pid计算
/// 使用方法：
/// 1. 创建pid对象，并初始化
///   pid_position_t pid;
///   pid_position_init(&pid, 0.1, 0.01, 0.001, 100);
/// 2. pid计算，并应用于系统（这里以电机设置占空比为例）
///   int duty = pid_position_calculate(&pid, current_value);
///   motor_set_duty(duty);
///
/// @version 0.1
/// @date 2024-08-12
///
/// @copyright Copyright (c) 2024
///

#ifndef MYLIB_UTILITY_PID_H
#define MYLIB_UTILITY_PID_H


/// @brief 位置式pid对象
///
typedef struct
{
    float target;       // 设定目标值
    float kp;           // 比例系数
    float ki;           // 积分系数
    float kd;           // 微分系数
    float last_error;   // 最后一次的误差
    float prev_error;   // 上一次误差
    float sum_error;    // 误差和
} pid_position_t;

/// @brief 位置式pid结构体初始化
/// @param pid pid对象
/// @param kp 比例系数
/// @param ki 积分系数
/// @param kd 微分系数
/// @param target 目标值
void pid_position_init(pid_position_t* pid, float kp, float ki, float kd, float target);

/// @brief 位置式pid设置目标值
/// @param pid pid对象
/// @param target 目标值
inline static void pid_position_set_target(pid_position_t* pid, float target)
{
    pid->target = target;
}

/// @brief 位置式pid计算
/// @param pid pid对象
/// @param current_value 当前pid输入值
/// @return 位置式pid计算后的u(k)
float pid_position_calculate(pid_position_t* pid, float current_value);

/// @brief 增量式pid对象
///
typedef struct
{
    float target;       // 设定目标值
    float kp;           // 比例系数
    float ki;           // 积分系数 ki=kp*T/Ti
    float kd;           // 微分系数 kd=kp*Td/T
    float last_error;   // 最后一次的误差
    float prev_error;   // 上一次误差
} pid_incremental_t;

/// @brief 增量式pid结构体初始化
/// @param pid pid对象
/// @param kp 比例系数
/// @param ki 积分系数
/// @param kd 微分系数
/// @param target 目标值
void pid_incremental_init(pid_incremental_t* pid, float kp, float ki, float kd, float target);

/// @brief 增量式pid设置目标值
/// @param pid pid对象
/// @param target 目标值
inline static void pid_incremental_set_target(pid_incremental_t* pid, float target)
{
    pid->target = target;
}

/// @brief 增量式pid计算
/// @param pid pid对象
/// @param current_value 当前pid输入值
/// @return 增量式pid计算后的△u(k)
float pid_incremental_calculate(pid_incremental_t* pid, float current_value);

#endif   // !MYLIB_UTILITY_PID_H
