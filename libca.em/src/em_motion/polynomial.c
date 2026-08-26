/// @file polynomial.c
/// @author Canrad
/// @brief 多项式轨迹规划基础实现
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///

#include "polynomial.h"

void poly5_generate_simple(f32 start, f32 end, f32 total_time, poly5_coeff_t* coeff)
{
    f32 d;
    f32 t2;
    f32 t3;
    f32 t4;
    f32 t5;

    if (!coeff || total_time <= 0.0f) {
        return;
    }

    d  = end - start;
    t2 = total_time * total_time;
    t3 = t2 * total_time;
    t4 = t3 * total_time;
    t5 = t4 * total_time;

    coeff->a0 = start;
    coeff->a1 = 0.0f;
    coeff->a2 = 0.0f;
    coeff->a3 = 10.0f * d / t3;
    coeff->a4 = -15.0f * d / t4;
    coeff->a5 = 6.0f * d / t5;
}

f32 poly5_eval_pos(const poly5_coeff_t* coeff, f32 t)
{
    if (!coeff) {
        return 0.0f;
    }

    return coeff->a0 +
           t * (coeff->a1 + t * (coeff->a2 + t * (coeff->a3 + t * (coeff->a4 + t * coeff->a5))));
}
