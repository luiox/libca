/// @file s_curve.h
/// @author Canrad
/// @brief S 曲线轨迹规划接口
/// @version 0.2
/// @date 2026-03-21
///
/// @copyright Copyright (c) 2026
///
/// S曲线是一种七段速度规划方法，通过限制加加速度(Jerk)实现平滑运动。
/// 七段分别为：加加速、匀加速、减加速、匀速、加减速、匀减速、减减速。
/// 当位移不足时，会自动退化为梯形或三角曲线。

#ifndef LIBCA_EM_MOTION_S_CURVE_H
#define LIBCA_EM_MOTION_S_CURVE_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum s_curve_segment_e
{
    S_CURVE_SEG_ACCEL_JERK_UP = 0,
    S_CURVE_SEG_ACCEL_CONST,
    S_CURVE_SEG_ACCEL_JERK_DOWN,
    S_CURVE_SEG_VEL_CONST,
    S_CURVE_SEG_DECEL_JERK_UP,
    S_CURVE_SEG_DECEL_CONST,
    S_CURVE_SEG_DECEL_JERK_DOWN,
    S_CURVE_SEG_COUNT
} s_curve_segment_t;

typedef struct s_curve_config_s
{
    f32 start;
    f32 end;
    f32 max_vel;
    f32 max_acc;
    f32 max_jerk;
} s_curve_config_t;

typedef struct s_curve_s
{
    s_curve_config_t cfg;

    f32 T[S_CURVE_SEG_COUNT];
    f32 total_time;

    f32 a_limit;
    f32 v_limit;
} s_curve_t;

void s_curve_init(s_curve_t* plan, const s_curve_config_t* cfg);

f32 s_curve_update(const s_curve_t* plan, f32 t);

f32 s_curve_get_velocity(const s_curve_t* plan, f32 t);

f32 s_curve_get_acceleration(const s_curve_t* plan, f32 t);

#ifdef __cplusplus
}
#endif

#endif