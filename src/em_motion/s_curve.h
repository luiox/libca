/**
 * @file s_curve.h
 * @author canrad (1517807724@qq.com)
 * @brief S 曲线轨迹规划接口（预留）
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_MOTION_S_CURVE_H
#define LIBCA_EM_MOTION_S_CURVE_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief S 曲线规划参数
 */
typedef struct s_curve_config_s {
    f32 start;
    f32 end;
    f32 max_vel;
    f32 max_acc;
    f32 max_jerk;
} s_curve_config_t;

/**
 * @brief S 曲线规划状态
 */
typedef struct s_curve_s {
    s_curve_config_t cfg;
    f32 total_time;
} s_curve_t;

/**
 * @brief 初始化 S 曲线规划器（当前为占位实现）
 */
void s_curve_init(s_curve_t* plan, const s_curve_config_t* cfg);

/**
 * @brief 查询 S 曲线目标位置（当前为占位实现）
 */
f32 s_curve_update(const s_curve_t* plan, f32 t);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_S_CURVE_H
