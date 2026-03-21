/**
 * @file polynomial.h
 * @author canrad (1517807724@qq.com)
 * @brief 多项式轨迹规划接口（预留）
 * @version 0.1
 * @date 2026-03-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_MOTION_POLYNOMIAL_H
#define LIBCA_EM_MOTION_POLYNOMIAL_H

#include <em_base/datatype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 五次多项式参数
 */
typedef struct poly5_coeff_s {
    f32 a0;
    f32 a1;
    f32 a2;
    f32 a3;
    f32 a4;
    f32 a5;
} poly5_coeff_t;

/**
 * @brief 生成五次多项式系数（边界速度/加速度为 0 的简化版本）
 *
 * @param start 起点位置
 * @param end 终点位置
 * @param total_time 总时长（s）
 * @param coeff 输出系数
 */
void poly5_generate_simple(f32 start, f32 end, f32 total_time, poly5_coeff_t* coeff);

/**
 * @brief 计算五次多项式位置
 *
 * @param coeff 多项式系数
 * @param t 时刻（s）
 * @return f32 位置
 */
f32 poly5_eval_pos(const poly5_coeff_t* coeff, f32 t);

#ifdef __cplusplus
}
#endif

#endif // !LIBCA_EM_MOTION_POLYNOMIAL_H
