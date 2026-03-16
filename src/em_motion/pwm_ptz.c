/**
 * @file pwm_ptz.c
 * @author canrad (1517807724@qq.com)
 * @brief 基于 PWM 舵机的 PTZ 控制封装基础实现
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "pwm_ptz.h"

static f32 clamp_angle(f32 v, f32 min_v, f32 max_v)
{
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

void pwm_ptz_init(
    pwm_ptz_t* ptz,
    const ptz_driver_t* drv,
    void* drv_ctx,
    const pwm_ptz_limits_t* limits)
{
    if (!ptz) {
        return;
    }

    ptz->drv = drv;
    ptz->drv_ctx = drv_ctx;
    ptz->target_pan = 0.0f;
    ptz->target_tilt = 0.0f;

    if (limits) {
        ptz->limits = *limits;
    } else {
        ptz->limits.pan.min_angle = -3.1415926f;
        ptz->limits.pan.max_angle = 3.1415926f;
        ptz->limits.tilt.min_angle = -1.5707963f;
        ptz->limits.tilt.max_angle = 1.5707963f;
    }

    if (ptz->drv && ptz->drv->init) {
        (void)ptz->drv->init(ptz->drv_ctx);
    }
}

void pwm_ptz_enable(pwm_ptz_t* ptz, bool enable)
{
    if (!ptz || !ptz->drv || !ptz->drv->enable) {
        return;
    }
    (void)ptz->drv->enable(ptz->drv_ctx, enable);
}

void pwm_ptz_set_target(pwm_ptz_t* ptz, f32 pan, f32 tilt)
{
    if (!ptz) {
        return;
    }

    ptz->target_pan = clamp_angle(pan, ptz->limits.pan.min_angle, ptz->limits.pan.max_angle);
    ptz->target_tilt = clamp_angle(tilt, ptz->limits.tilt.min_angle, ptz->limits.tilt.max_angle);

    if (ptz->drv && ptz->drv->set_angle) {
        (void)ptz->drv->set_angle(ptz->drv_ctx, ptz->target_pan, ptz->target_tilt);
    }
}
