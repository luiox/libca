#include "filter.h"
#include <em_base/datatype.h>
#include "math_util.h"
#include <em_base/debug.h>
#include <em_base/memory_util.h>
#include <math.h>

// ============================================
// 二阶巴特沃斯低通滤波器实现
// ============================================

void butterworth2_filter_init(butterworth2_filter_t* self, f32 sample_freq, f32 cutoff_freq)
{
    // 清空历史状态
    self->x_prev1 = 0;
    self->x_prev2 = 0;
    self->y_prev1 = 0;
    self->y_prev2 = 0;

    if (cutoff_freq <= 0.0f) {
        // 无滤波：直通 (y = x)
        self->b0 = 1.0f;
        self->b1 = 0.0f;
        self->b2 = 0.0f;
        self->a1 = 0.0f;
        self->a2 = 0.0f;
        return;
    }

    // 计算系数 (双线性变换法)
    // ohm = tan(pi * fc / fs)
    // M_PI_F 应在 math_util.h 中定义，若无则使用标准值
#ifndef M_PI_F
#    define M_PI_F 3.14159265f
#endif

    f32 fr  = sample_freq / cutoff_freq;
    f32 ohm = tanf(M_PI_F / fr);
    f32 c   = 1.0f + 2.0f * cosf(M_PI_F / 4.0f) * ohm + ohm * ohm;

    self->b0 = ohm * ohm / c;
    self->b1 = 2.0f * self->b0;
    self->b2 = self->b0;

    // a0 = 1.0 (已归一化)
    self->a1 = 2.0f * (ohm * ohm - 1.0f) / c;
    self->a2 = (1.0f - 2.0f * cosf(M_PI_F / 4.0f) * ohm + ohm * ohm) / c;
}

f32 butterworth2_filter_update(butterworth2_filter_t* self, f32 input)
{
    // 差分方程:
    // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]

    f32 output = self->b0 * input + self->b1 * self->x_prev1 + self->b2 * self->x_prev2 -
                 self->a1 * self->y_prev1 - self->a2 * self->y_prev2;

    // 简单的数值稳定性检查 (NaN / Inf)
    if (isnan(output) || isinf(output)) {
        // 发生异常时重置状态并直通
        output        = input;
        self->x_prev1 = input;
        self->x_prev2 = input;
        self->y_prev1 = input;
        self->y_prev2 = input;
    }
    else {
        // 更新历史状态
        self->x_prev2 = self->x_prev1;
        self->x_prev1 = input;

        self->y_prev2 = self->y_prev1;
        self->y_prev1 = output;
    }

    return output;
}

void mean_filter_u16_init(mean_filter_u16_t* self, u16* buffer, u16 size)
{
    self->buffer = buffer;
    self->size   = size;
    self->index  = 0;
    self->sum    = 0;
    self->count  = 0;

    // 清空buffer
    for (u16 i = 0; i < size; i++) {
        self->buffer[i] = 0;
    }
}

u16 mean_filter_u16_update(mean_filter_u16_t* self, u16 input)
{
    if (self->size == 0)
        return input;

    // 减去即将被覆盖的旧值 (仅当buffer已满时)
    if (self->count == self->size) {
        self->sum -= self->buffer[self->index];
    }
    else {
        self->count++;
    }

    // 写入新值
    self->buffer[self->index] = input;
    self->sum += input;

    // 移动索引
    self->index++;
    if (self->index >= self->size) {
        self->index = 0;
    }

    return (u16)(self->sum / self->count);
}

void median_filter_u16_init(median_filter_u16_t* self, u16* buffer, u16 size)
{
    self->buffer = buffer;
    self->size   = size;
    self->index  = 0;
    self->count  = 0;
}

u16 median_filter_u16_update(median_filter_u16_t* self, u16 input)
{
    if (self->size == 0)
        return input;

    // 存入新值
    self->buffer[self->index] = input;

    // 更新计数
    if (self->count < self->size) {
        self->count++;
    }

    // 更新索引
    self->index++;
    if (self->index >= self->size) {
        self->index = 0;
    }

    // 必须拷贝一份数据进行排序，保持原Buffer的时序性
    // 使用 VLA (C99), 注意栈空间
    u16 temp[64];
    u16 valid_len = self->count;

    // 如果窗口太大，截断或者只处理前64个 (保护机制)
    if (valid_len > 64) {
        valid_len = 64;
        debug_print("[median_filter] Window size too large, truncating to 64");
    }

    mem_cpy(temp, self->buffer, valid_len * sizeof(u16));

    // 冒泡排序
    for (u16 i = 0; i < valid_len - 1; i++) {
        for (u16 j = 0; j < valid_len - 1 - i; j++) {
            if (temp[j] > temp[j + 1]) {
                u16 swap    = temp[j];
                temp[j]     = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }

    // 取中值
    if (valid_len % 2 == 0) {
        return (temp[valid_len / 2 - 1] + temp[valid_len / 2]) / 2;
    }
    else {
        return temp[valid_len / 2];
    }
}
