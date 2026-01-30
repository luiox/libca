#include "filter.h"
#include "../em_base/datatype.h"
#include "../em_util/math_util.h"
#include "debug.h"
#include "memory_util.h"

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
#define M_PI_F 3.14159265f
#endif

    f32 fr = sample_freq / cutoff_freq;
    f32 ohm = tanf(M_PI_F / fr);
    f32 c = 1.0f + 2.0f * cosf(M_PI_F / 4.0f) * ohm + ohm * ohm;

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
    
    f32 output = self->b0 * input + 
                 self->b1 * self->x_prev1 + 
                 self->b2 * self->x_prev2 - 
                 self->a1 * self->y_prev1 - 
                 self->a2 * self->y_prev2;

    // 简单的数值稳定性检查 (NaN / Inf)
    if (isnan(output) || isinf(output)) {
        // 发生异常时重置状态并直通
        output = input;
        self->x_prev1 = input;
        self->x_prev2 = input;
        self->y_prev1 = input;
        self->y_prev2 = input;
    } else {
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
    self->size = size;
    self->index = 0;
    self->sum = 0;
    self->count = 0;
    
    // 清空buffer
    for(u16 i = 0; i < size; i++) {
        self->buffer[i] = 0;
    }
}

u16 mean_filter_u16_update(mean_filter_u16_t* self, u16 input)
{
    if (self->size == 0) return input;

    // 减去即将被覆盖的旧值 (仅当buffer已满时)
    if (self->count == self->size) {
        self->sum -= self->buffer[self->index];
    } else {
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
    self->size = size;
    self->index = 0;
    self->count = 0;
}

u16 median_filter_u16_update(median_filter_u16_t* self, u16 input)
{
    if (self->size == 0) return input;

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
                u16 swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }

    // 取中值
    if (valid_len % 2 == 0) {
        return (temp[valid_len / 2 - 1] + temp[valid_len / 2]) / 2;
    } else {
        return temp[valid_len / 2];
    }
}

#if TEST_ENABLE
#include "../em_test/test.h"

TEST_CASE(test_butterworth2) {
    butterworth2_filter_t lpf;
    
    // Test 1: Cutoff <= 0 -> Passthrough
    butterworth2_filter_init(&lpf, 100.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, butterworth2_filter_update(&lpf, 10.0f));
    TEST_ASSERT_EQUAL_FLOAT(20.0f, butterworth2_filter_update(&lpf, 20.0f));

    // Test 2: Standard Filtering (100Hz Sample, 10Hz Cutoff)
    butterworth2_filter_init(&lpf, 100.0f, 10.0f);
    
    // Simulate step response 0 -> 100
    // Initial output should be small and gradually increase
    f32 out = butterworth2_filter_update(&lpf, 100.0f);
    TEST_ASSERT_TRUE(out < 100.0f);
    TEST_ASSERT_TRUE(out > 0.0f);

    // After many iterations, it should converge to 100
    for (int i = 0; i < 100; i++) {
        out = butterworth2_filter_update(&lpf, 100.0f);
    }
    // Check convergence (allow small error)
    TEST_ASSERT_TRUE(out > 99.0f && out < 101.0f);
}

TEST_CASE(test_mean_filter_u16) {
    mean_filter_u16_t filter;
    u16 buffer[4]; // Size 4
    
    mean_filter_u16_init(&filter, buffer, 4);

    // [10, 0, 0, 0] -> avg = 10/1 = 10
    TEST_ASSERT_EQUAL_INT(10, mean_filter_u16_update(&filter, 10));
    
    // [10, 20, 0, 0] -> avg = 30/2 = 15
    TEST_ASSERT_EQUAL_INT(15, mean_filter_u16_update(&filter, 20));
    
    // [10, 20, 30, 0] -> avg = 60/3 = 20
    TEST_ASSERT_EQUAL_INT(20, mean_filter_u16_update(&filter, 30));
    
    // [10, 20, 30, 40] -> avg = 100/4 = 25
    TEST_ASSERT_EQUAL_INT(25, mean_filter_u16_update(&filter, 40));
    
    // [50, 20, 30, 40] -> sum was 100, remove 10 add 50 -> 140/4 = 35
    TEST_ASSERT_EQUAL_INT(35, mean_filter_u16_update(&filter, 50));
}

TEST_CASE(test_median_filter_u16) {
    median_filter_u16_t filter;
    u16 buffer[5]; // Size 5 (Odd)
    
    median_filter_u16_init(&filter, buffer, 5);

    // [10] -> median 10
    TEST_ASSERT_EQUAL_INT(10, median_filter_u16_update(&filter, 10));
    
    // [10, 20] -> sorted [10, 20] -> median (10+20)/2 = 15
    TEST_ASSERT_EQUAL_INT(15, median_filter_u16_update(&filter, 20));
    
    // [10, 20, 5] -> sorted [5, 10, 20] -> median 10
    TEST_ASSERT_EQUAL_INT(10, median_filter_u16_update(&filter, 5));
    
    // [10, 20, 5, 100] -> sorted [5, 10, 20, 100] -> median (10+20)/2 = 15
    TEST_ASSERT_EQUAL_INT(15, median_filter_u16_update(&filter, 100));
    
    // [10, 20, 5, 100, 1] -> sorted [1, 5, 10, 20, 100] -> median 10
    TEST_ASSERT_EQUAL_INT(10, median_filter_u16_update(&filter, 1));
    
    // Full buffer, replace oldest (10) with 50
    // [50, 20, 5, 100, 1] -> sorted [1, 5, 20, 50, 100] -> median 20
    TEST_ASSERT_EQUAL_INT(20, median_filter_u16_update(&filter, 50));
}
#endif

