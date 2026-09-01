/* Auto-migrated from src/em_util/filter.c test blocks */
#include "filter.h"
#include <em_base/datatype.h>
#include "math_util.h"
#include <em_base/debug.h>
#include <em_base/memory_util.h>
#include <math.h>

#include <em_test/test.h>

TEST_CASE(test_butterworth2)
{
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

TEST_CASE(test_mean_filter_u16)
{
    mean_filter_u16_t filter;
    u16               buffer[4];   // Size 4

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

TEST_CASE(test_median_filter_u16)
{
    median_filter_u16_t filter;
    u16                 buffer[5];   // Size 5 (Odd)

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
