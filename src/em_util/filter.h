#ifndef LIBCA_EM_UTIL_FILTER_H
#define LIBCA_EM_UTIL_FILTER_H

#include "../em_base/datatype.h"

// ============================================
// 二阶巴特沃斯低通滤波器
// ============================================
typedef struct butterworth2_filter {
    // 滤波器系数 (由初始化函数计算)
    f32 b0;
    f32 b1;
    f32 b2;
    f32 a1;
    f32 a2;

    // 历史状态 (x:输入, y:输出)
    f32 x_prev1; // x[n-1]
    f32 x_prev2; // x[n-2]
    f32 y_prev1; // y[n-1]
    f32 y_prev2; // y[n-2]
} butterworth2_filter_t;

/**
 * @brief 初始化二阶巴特沃斯低通滤波器
 * 
 * @param self 滤波器对象
 * @param sample_freq 采样频率 (Hz)
 * @param cutoff_freq 截止频率 (Hz)
 */
void butterworth2_filter_init(butterworth2_filter_t* self, f32 sample_freq, f32 cutoff_freq);

/**
 * @brief 更新滤波器并获取输出
 * 
 * @param self 滤波器对象
 * @param input 当前输入值
 * @return f32 滤波后输出值
 */
f32 butterworth2_filter_update(butterworth2_filter_t* self, f32 input);

///////////////////////////////////////////////////////////////////////////////
// ============================================
// 通用滤波器 (u16版本, 适用于ADC/编码器等)
// ============================================

// ============================================
// 滑动平均滤波器
// ============================================
typedef struct mean_filter_u16 {
    u16* buffer;      // 历史数据缓存 (外部传入)
    u16  size;        // 缓存大小
    u16  index;       // 当前写入位置
    u32  sum;         // 当前和 (缓存加速)
    u16  count;       // 当前已有数据量
} mean_filter_u16_t;

/**
 * @brief 初始化滑动平均滤波器
 * 
 * @param self 滤波器对象
 * @param buffer 数据缓存数组 (大小由 size 指定)
 * @param size 缓存大小 (例如 5, 10)
 */
void mean_filter_u16_init(mean_filter_u16_t* self, u16* buffer, u16 size);

/**
 * @brief 更新并获取滑动平均值
 * 
 * @param self 滤波器对象
 * @param input 新采样值
 * @return u16 滤波后结果
 */
u16 mean_filter_u16_update(mean_filter_u16_t* self, u16 input);

// ============================================
// 中值滤波器
// ============================================
typedef struct median_filter_u16 {
    u16* buffer;      // 历史数据缓存 (外部传入)
    u16  size;        // 缓存大小
    u16  index;       // 当前写入位置
    u16  count;       // 当前已有数据量
} median_filter_u16_t;

/**
 * @brief 初始化中值滤波器
 * 
 * @param self 滤波器对象
 * @param buffer 数据缓存数组
 * @param size 缓存大小 (建议奇数, 如 5, 7, 9)
 */
void median_filter_u16_init(median_filter_u16_t* self, u16* buffer, u16 size);

/**
 * @brief 更新并获取中值
 * 注意: 内部会使用栈空间进行排序，窗口不宜过大 (建议 < 64)
 * 
 * @param self 滤波器对象
 * @param input 新采样值
 * @return u16 中值结果
 */
u16 median_filter_u16_update(median_filter_u16_t* self, u16 input);

#endif // !LIBCA_EM_UTIL_FILTER_H
