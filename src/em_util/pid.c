/**
 * @file pid.c
 * @author canrad (1517807724@qq.com)
 * @brief pid算法，包含位置式和增量式pid计算
 * @version 0.1
 * @date 2024-08-12
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "pid.h"

void pid_position_init(pid_position_t* pid, float kp, float ki, float kd, float target)
{
    pid->target     = target;
    pid->kp         = kp;
    pid->ki         = ki;
    pid->kd         = kd;
    pid->last_error = 0.0;
    pid->prev_error = 0.0;
    pid->sum_error  = 0.0;
}

float pid_position_calculate(pid_position_t* pid, float current_value)
{
    // 离散的位置式PID计算公式
    // u(k)=Kp*e(k)+Ki*∑e(j)+Kd[e(k)-e(k-1)]
    // 其中：
    // u(k)为第k次采样时刻的PID控制输出
    // e(k)为第k次采样时刻输入的偏差值，e(k)=r(k)-y(k)
    // r(k)为第k次采样时刻的设定值
    // y(k)为第k次采样时刻的反馈值

    float error = pid->target - current_value;   // 当前误差 = 目标值 - 当前值
    pid->sum_error += error;                     // 误差积分
    float d_error = error - pid->last_error;     // 误差微分

    pid->prev_error = pid->last_error;   // 更新上一次的误差
    pid->last_error = error;             // 更新当前最新的误差

    return pid->kp * error              // 比例项
           + pid->ki * pid->sum_error   // 积分项
           + pid->kd * d_error;         // 微分项
}

void pid_incremental_init(pid_incremental_t* pid, float kp, float ki, float kd, float target)
{
    pid->target     = target;
    pid->kp         = kp;
    pid->ki         = ki;
    pid->kd         = kd;
    pid->last_error = 0.0;
    pid->prev_error = 0.0;
}

float pid_incremental_calculate(pid_incremental_t* pid, float current_value)
{
    // 离散的增量式PID计算公式
    // 从位置式PID的公式中得到u(k)，带入k-1，得到u(k-1)
    // 得到△u(k)=u(k)-u(k-1)
    // △u(k)=Ae(k)-Be(k-1)+Ce(k-2)
    // 其中：
    // A=Kp+Ki+Kd
    // B=Kp+2Kd
    // C=Kd
    // 加上u(k)=u(k-1)+△u(k)，再展开，得到增量式PID公式如下
    // u(k)=u(k-1)+Kp[e(k)-e(k-1)]+Ki*e(k)+Kd[e(k)-2e(k-1)+e(k-2)]

    // 注意pid_incremental_calculate这个函数计算的是△u(k)，也就是增量，而不是u(k)

    float error  = pid->target - current_value;   // 当前误差 = 目标值 - 当前值
    float output = pid->kp * (error - pid->last_error)     // Kp[e(k)-e(k-1)]
                   + pid->ki * error                       // +Ki*e(k)
                   + pid->kd * (error - 2 * pid->last_error + pid->prev_error); // +Kd[e(k)-2e(k-1)+e(k-2)]
    pid->prev_error = pid->last_error;
    pid->last_error = error;

    return output;
}

#if TEST_ENABLE

#include "../em_test/test.h"

TEST_CASE(pid_position_basic)
{
    pid_position_t pid;
    pid_position_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);   // 目标10
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};
    
    // 简单验证第一个计算结果
    // error = 10 - 0 = 10
    // sum_error = 10
    // d_error = 10 - 0 = 10
    // output = 1.0*10 + 0.1*10 + 0.05*10 = 10 + 1 + 0.5 = 11.5
    float u = pid_position_calculate(&pid, values[0]);
    TEST_ASSERT_EQUAL_FLOAT(11.5f, u);
}

TEST_CASE(pid_incremental_basic)
{
    pid_incremental_t pid;
    pid_incremental_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};
    
    // 简单验证第一个增量结果
    // error = 10 - 0 = 10
    // last_error = 0, prev_error = 0
    // du = 1.0*(10-0) + 0.1*10 + 0.05*(10 - 2*0 + 0) = 10 + 1 + 0.5 = 11.5
    float du = pid_incremental_calculate(&pid, values[0]);
    TEST_ASSERT_EQUAL_FLOAT(11.5f, du);
}

#endif
