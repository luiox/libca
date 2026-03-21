/**
 * @file pwm_ptz.h
 * @author canrad (1517807724@qq.com)
 * @brief 基于 PWM 舵机的 PTZ 控制封装
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_MOTION_PWM_PTZ_H
#define LIBCA_EM_MOTION_PWM_PTZ_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PTZ 驱动抽象接口
 */
typedef struct ptz_driver_s
{
    /**
     * @brief 初始化驱动
     *
     * @param ctx 驱动上下文
     * @return i32 0 表示成功
     */
    i32 (*init)(void* ctx);

    /**
     * @brief 使能或失能
     *
     * @param ctx 驱动上下文
     * @param enable true 使能，false 失能
     * @return i32 0 表示成功
     */
    i32 (*enable)(void* ctx, bool enable);

    /**
     * @brief 设置 PTZ 双轴角度
     *
     * @param ctx 驱动上下文
     * @param pan 水平轴角度，单位 rad
     * @param tilt 俯仰轴角度，单位 rad
     * @return i32 0 表示成功
     */
    i32 (*set_angle)(void* ctx, f32 pan, f32 tilt);
} ptz_driver_t;

/**
 * @brief 单轴角度范围限制
 */
typedef struct ptz_axis_limit_s
{
    f32 min_angle;
    f32 max_angle;
} ptz_axis_limit_t;

/**
 * @brief 双轴 PTZ 限位配置
 */
typedef struct pwm_ptz_limits_s
{
    ptz_axis_limit_t pan;
    ptz_axis_limit_t tilt;
} pwm_ptz_limits_t;

/**
 * @brief PWM PTZ 控制对象
 */
typedef struct pwm_ptz_s
{
    const ptz_driver_t* drv;
    void*               drv_ctx;
    pwm_ptz_limits_t    limits;
    f32                 target_pan;
    f32                 target_tilt;
} pwm_ptz_t;

/**
 * @brief 初始化 PWM PTZ 控制对象
 *
 * @param ptz 控制对象
 * @param drv 驱动接口
 * @param drv_ctx 驱动上下文
 * @param limits 限位配置
 */
void pwm_ptz_init(pwm_ptz_t* ptz, const ptz_driver_t* drv, void* drv_ctx,
                  const pwm_ptz_limits_t* limits);

/**
 * @brief 使能或失能 PTZ
 *
 * @param ptz 控制对象
 * @param enable true 使能，false 失能
 */
void pwm_ptz_enable(pwm_ptz_t* ptz, bool enable);

/**
 * @brief 设置云台目标角度
 *
 * @param ptz 控制对象
 * @param pan 水平轴目标角度（rad）
 * @param tilt 俯仰轴目标角度（rad）
 */
void pwm_ptz_set_target(pwm_ptz_t* ptz, f32 pan, f32 tilt);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_MOTION_PWM_PTZ_H
