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

#include "../em_base/compiler_helper.h"
#include "../em_base/datatype.h"
#include <stdbool.h>
#include <math.h>
#include "../em_base/debug.h"  

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



/**
 * @brief 原子操作代码块
 */
#define SAFE_ATOM_CODE                                 \
  using(uint32_t SAFE_NAME(temp) = ({                  \
          uint32_t SAFE_NAME(temp2) = __get_PRIMASK(); \
          __disable_irq();                             \
          SAFE_NAME(temp2);                            \
        }),                                            \
        __set_PRIMASK(SAFE_NAME(temp)))


#define __IRQ_SAFE SAFE_ATOM_CODE

#define __dim_of_1(__array) (sizeof(__array) / sizeof(__array[0]))
#define __dim_of_2(__array, __type) (sizeof(__array) / sizeof(__type))
/**
 * @brief 获取数组长度
 * @param __array 数组
 * @param __type 元素类型 (可选)
 */
#define dimof(...) EVAL(__dim_of_, __VA_ARGS__)(__VA_ARGS__)

#define __foreach_2(__array, __type)                              \
  USING(__type * _ = __array) for (uint_fast32_t SAFE_NAME(cnt) = \
                                       dimof(__array, __type);    \
                                   SAFE_NAME(cnt) > 0; _++, SAFE_NAME(cnt)--)

#define __foreach_1(__array) __foreach_2(__array, typeof(*(__array)))
#define __foreach_3(__array, __type, __pt)                          \
  USING(__type * __pt =                                             \
            __array) for (uint_fast32_t CONNECT2(count, __LINE__) = \
                              dimof(__array, __type);               \
                          SAFE_NAME(cnt) > 0; __pt++, SAFE_NAME(cnt)--)
#define __foreach_reverse_2(__array, __type)                \
  USING(__type * _ = __array + dimof(__array, __type) -     \
                     1) for (uint_fast32_t SAFE_NAME(cnt) = \
                                 dimof(__array, __type);    \
                             SAFE_NAME(cnt) > 0; _--, SAFE_NAME(cnt)--)
#define __foreach_reverse_1(__array) \
  __foreach_reverse_2(__array, typeof(*(__array)))
#define __foreach_reverse_3(__array, __type, __pt)             \
  USING(__type * __pt = __array + dimof(__array, __type) -     \
                        1) for (uint_fast32_t SAFE_NAME(cnt) = \
                                    dimof(__array, __type);    \
                                SAFE_NAME(cnt) > 0; __pt--, SAFE_NAME(cnt)--)
/**
 * @brief 遍历数组
 * @param __array 数组
 * @param __type 元素类型 (可选)
 * @param __pt 元素指针名 (可选)
 */
#define foreach(...) EVAL(__foreach_, __VA_ARGS__)(__VA_ARGS__)

/**
 * @brief 反向遍历数组
 * @param __array 数组
 * @param __type 元素类型 (可选)
 * @param __pt 元素指针名 (可选)
 */
#define foreach_reverse(...) EVAL(__foreach_reverse_, __VA_ARGS__)(__VA_ARGS__)

/**
 * @brief Get the absolute value of the specified value
 */
#define CABS(x) ((x) >= 0 ? (x) : -(x))

#define __MIN_2(__a, __b) ((__a) < (__b) ? (__a) : (__b))
#define __MIN_3(__a, __b, __c) __MIN_2(__MIN_2(__a, __b), __c)
#define __MIN_4(__a, __b, __c, __d) \
  __MIN_2(__MIN_2(__a, __b), __MIN_2(__c, __d))

/**
 * @brief Get the minimum value of the specified values
 */
#define CMIN(...) EVAL(__MIN_, __VA_ARGS__)(__VA_ARGS__)

#define __MAX_2(__a, __b) ((__a) > (__b) ? (__a) : (__b))
#define __MAX_3(__a, __b, __c) __MAX_2(__MAX_2(__a, __b), __c)
#define __MAX_4(__a, __b, __c, __d) \
  __MAX_2(__MAX_2(__a, __b), __MAX_2(__c, __d))

/**
 * @brief Get the maximum value of the specified values
 */
#define CMAX(...) EVAL(__MAX_, __VA_ARGS__)(__VA_ARGS__)

/**
 * @brief Round a float to the nearest integer
 */
#define ROUND(__f) ((int)((__f) + 0.5f))

/**
 * @brief Clamp a value to the specified range
 */
#define CLAMP(__x, __min, __max) CMIN(CMAX(__x, __min), __max)

/**
 * @brief Linear mapping input to the specified range
 */
#define MAP(__x, __in_min, __in_max, __out_min, __out_max)              \
  ((__x - __in_min) * (__out_max - __out_min) / (__in_max - __in_min) + \
   __out_min)

/**
 * @brief make compiler know the expression is likely to be true
 */
#define likeyly(x) __builtin_expect(!!(x), 1)

/**
 * @brief make compiler know the expression is likely to be false
 */
#define unlikely(x) __builtin_expect(!!(x), 0)

#endif   // !MATH_UTIL_H
