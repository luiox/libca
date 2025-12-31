/**
 * @file math_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 数学相关的工具函数，补充标准库的不足
 * @version 0.1
 * @date 2025-07-25
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include "compiler_helper.h"
#include "datatype.h"
#include <stdbool.h>
#include <math.h>
#include "debug.h"  

typedef struct
{
    float x;
    float y;
} vec2f;

typedef struct
{
    float x;
    float y;
    float z;
} vec3f_t;

// 最小值
#define math_min(a, b) ((a) < (b) ? (a) : (b))
// 最大值
#define math_max(a, b) ((a) > (b) ? (a) : (b))
// 绝对值
#define math_abs(x) ((x) < 0 ? -(x) : (x))
// 判断是否是2的幂
#define math_is_power_of_two(x) ((x) && !((x) & ((x) - 1)))
// 向上对齐
#define math_align_up(x, align) (((x) + (align) - 1) & ~((align) - 1))
// 向下对齐
#define math_align_down(x, align) ((x) & ~((align) - 1))

/**
 * @brief 计算整数的幂
 *
 * @param base 底数
 * @param exp 指数
 * @return int32_t 底数的指数次幂
 */
i32 math_pow_s32(i32 base, i32 exp);
/**
 * @brief 限幅函数，将输入值限制在min和max之间
 *
 * @param value 输入值
 * @param min 最大值
 * @param max 最小值
 * @return 限幅后的值
 */
float clampf(float value, float min, float max);

// 快速sin(x)（5阶泰勒展开法）
CA_FORCE_INLINE float fast_sinf(float x)
{
    // const float B = 4.0f / 3.14159265f;
    // const float C = -4.0f / (3.14159265f * 3.14159265f);
    const float B = 1.27324f;
    const float C = -0.405285f;
    // 二阶补偿
    const float P = 0.225f;

    float y = B * x + C * x * fabsf(x);

    y = P * (y * fabsf(y) - y) + y;
    return y;
}

CA_FORCE_INLINE float math_fabs(float x) {
#if defined(__IEEE_754__)
    union {
        float f;
        uint32_t u;
    } converter = {x};
    converter.u &= 0x7FFFFFFFu;
    return converter.f;
#else
    return (x < 0.0f) ? -x : x;
#endif
}

#ifndef M_PI_F
#    define M_PI_F 3.141592653589793f
#endif

#ifndef PI
#    define PI M_PI_F
#endif


#define DEG2RAD (PI / 180.0f)
#define RAD2DEG (180.0f / PI)



float constrain_float(float amt, float low, float high);
float sq(float v);
float safe_sqrt(float v);


float invSqrt(float x);

void  FastSinCos(float x, float* sinVal, float* cosVal);
float FastSin(float x);
float FastCos(float x);


float FastSqrtI(float x);
float FastSqrt(float x);

int16_t constrain_int16(int16_t amt, int16_t low, int16_t high);


#define sq2(sq) (((float)sq) * ((float)sq))

#if DETECT_MODE

// 检查 float 是否符合 IEEE 754
static int is_float_ieee754() {
    float f = -1.0f;
    uint32_t u;
    memcpy(&u, &f, sizeof(f));
    return u == 0xBF800000; // -1.0 的 IEEE 754 单精度表示
}

// 检查 double 是否符合 IEEE 754
static int is_double_ieee754() {
    double d = -1.0;
    uint64_t u;
    memcpy(&u, &d, sizeof(d));
    return u == 0xBFF0000000000000ULL; // -1.0 的 IEEE 754 双精度表示
}

// 检查 NaN 和 Inf 的位模式
static int test_special_values() {
    float inf = 1.0f / 0.0f;
    float nan = 0.0f / 0.0f;
    uint32_t u_inf, u_nan;
    memcpy(&u_inf, &inf, sizeof(inf));
    memcpy(&u_nan, &nan, sizeof(nan));
    return (u_inf == 0x7F800000) && (u_nan != 0x7F800000);
}

static void ieee754_check(void)
{
   if (is_float_ieee754() && is_double_ieee754() && test_special_values()) {
        debug_print("This platform supports IEEE 754 standard.\n");
    } else {
        debug_print("This platform does NOT support IEEE 754 standard.\n");
    }
}

#endif

#endif   // !MATH_UTIL_H
