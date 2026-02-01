/**
 * @file motor.h
 * @author canrad (1517807724@qq.com)
 * @brief 直流有刷电机驱动 (PWM调速)
 * @version 0.1
 * @date 2026-02-01
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_DRIVER_MOTOR_H
#define LIBCA_EM_DRIVER_MOTOR_H

#include "../em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Motor port 层接口
 *
 * 用于抽象不同平台的PWM控制接口
 */
typedef struct motor_port
{
    /**
     * @brief 设置PWM占空比
     *
     * @param htim PWM定时器句柄（平台相关）
     * @param channel PWM通道（某些平台有多个通道）
     * @param duty 占空比 (0-100)
     */
    void (*pwm_set_duty)(void* htim, u16 channel, u8 duty);

    /**
     * @brief 启动PWM输出
     *
     * @param htim PWM定时器句柄
     * @param channel PWM通道
     */
    void (*pwm_start)(void* htim, u16 channel);

    /**
     * @brief 停止PWM输出
     *
     * @param htim PWM定时器句柄
     * @param channel PWM通道
     */
    void (*pwm_stop)(void* htim, u16 channel);
} motor_port_t;

/**
 * @brief 绑定硬件接口
 *
 * @param port 接口结构体
 */
void motor_bind_port(const motor_port_t* port);

/**
 * @brief 检查接口是否已注册
 *
 * @return bool true 为已注册
 */
bool motor_port_is_registered(void);

/**
 * @brief Motor 对象结构体
 */
typedef struct motor
{
    void* htim;       // PWM定时器句柄（平台相关）
    u16  channel;     // PWM通道
    u8   duty;        // 当前占空比 (0-100)
    u8   running;     // 运行状态 (0-停止, 1-运行)
} motor_t;

/**
 * @brief Motor 错误码
 */
#define MOTOR_OK                     0
#define MOTOR_ERR_PORT_NOT_REGISTERED (-1)
#define MOTOR_ERR_INVALID_PARAM       (-2)

/**
 * @brief 初始化 Motor 对象
 *
 * @param self 对象指针
 * @param htim PWM定时器句柄（由使用者初始化）
 * @param channel PWM通道编号
 * @return i32 MOTOR_OK 或错误码
 */
i32 motor_init(motor_t* self, void* htim, u16 channel);

/**
 * @brief 设置PWM占空比
 *
 * @param self 对象指针
 * @param duty 占空比 (0-100)
 * @return i32 MOTOR_OK 或错误码
 */
i32 motor_set_duty(motor_t* self, u8 duty);

/**
 * @brief 获取当前占空比
 *
 * @param self 对象指针
 * @return u8 当前占空比 (0-100)
 */
u8 motor_get_duty(motor_t* self);

/**
 * @brief 启动电机
 *
 * @param self 对象指针
 * @return i32 MOTOR_OK 或错误码
 */
i32 motor_start(motor_t* self);

/**
 * @brief 停止电机
 *
 * @param self 对象指针
 * @return i32 MOTOR_OK 或错误码
 */
i32 motor_stop(motor_t* self);

/**
 * @brief 检查电机是否运行中
 *
 * @param self 对象指针
 * @return bool true 为运行中
 */
bool motor_is_running(motor_t* self);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_MOTOR_H
