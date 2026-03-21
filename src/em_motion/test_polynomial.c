/**
 * @file test_polynomial.c
 * @brief Unit tests for polynomial trajectory planner
 */

#define _USE_MATH_DEFINES
#include "test.h"
#include "polynomial.h"
#include <math.h>

#define EPSILON 1e-4f

TEST_CASE(test_poly5_generate_simple_basic)
{
    poly5_coeff_t coeff;

    poly5_generate_simple(0.0f, 10.0f, 2.0f, &coeff);

    TEST_EXPECT_EQ_F32_E(0.0f, coeff.a0, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, coeff.a1, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, coeff.a2, EPSILON);
    TEST_EXPECT_EQ_TRUE(coeff.a3 != 0.0f);
    TEST_EXPECT_EQ_TRUE(coeff.a4 != 0.0f);
    TEST_EXPECT_EQ_TRUE(coeff.a5 != 0.0f);
}

TEST_CASE(test_poly5_generate_simple_null_pointer)
{
    poly5_generate_simple(0.0f, 10.0f, 2.0f, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_poly5_generate_simple_invalid_time)
{
    poly5_coeff_t coeff = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    poly5_generate_simple(0.0f, 10.0f, 0.0f, &coeff);

    TEST_EXPECT_EQ_F32_E(1.0f, coeff.a0, EPSILON);
}

TEST_CASE(test_poly5_generate_simple_negative_time)
{
    poly5_coeff_t coeff = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    poly5_generate_simple(0.0f, 10.0f, -1.0f, &coeff);

    TEST_EXPECT_EQ_F32_E(1.0f, coeff.a0, EPSILON);
}

TEST_CASE(test_poly5_eval_pos_at_start)
{
    poly5_coeff_t coeff;

    poly5_generate_simple(0.0f, 10.0f, 2.0f, &coeff);

    f32 pos = poly5_eval_pos(&coeff, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_poly5_eval_pos_at_end)
{
    poly5_coeff_t coeff;
    f32           total_time = 2.0f;

    poly5_generate_simple(0.0f, 10.0f, total_time, &coeff);

    f32 pos = poly5_eval_pos(&coeff, total_time);

    TEST_EXPECT_EQ_F32_E(10.0f, pos, EPSILON);
}

TEST_CASE(test_poly5_eval_pos_null_pointer)
{
    f32 pos = poly5_eval_pos(NULL, 1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_poly5_eval_pos_negative_time)
{
    poly5_coeff_t coeff;

    poly5_generate_simple(0.0f, 10.0f, 2.0f, &coeff);

    f32 pos = poly5_eval_pos(&coeff, -1.0f);

    TEST_EXPECT_EQ_TRUE(pos < 0.0f);
}

TEST_CASE(test_poly5_eval_pos_midpoint)
{
    poly5_coeff_t coeff;
    f32           start      = 0.0f;
    f32           end        = 10.0f;
    f32           total_time = 2.0f;

    poly5_generate_simple(start, end, total_time, &coeff);

    f32 pos = poly5_eval_pos(&coeff, total_time * 0.5f);

    TEST_EXPECT_EQ_TRUE(pos > start);
    TEST_EXPECT_EQ_TRUE(pos < end);
}

TEST_CASE(test_poly5_backward_motion)
{
    poly5_coeff_t coeff;
    f32           start      = 10.0f;
    f32           end        = 0.0f;
    f32           total_time = 2.0f;

    poly5_generate_simple(start, end, total_time, &coeff);

    f32 pos_start = poly5_eval_pos(&coeff, 0.0f);
    f32 pos_end   = poly5_eval_pos(&coeff, total_time);

    TEST_EXPECT_EQ_F32_E(10.0f, pos_start, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, pos_end, EPSILON);
}

TEST_CASE(test_poly5_symmetric_path)
{
    poly5_coeff_t coeff;
    f32           start      = 0.0f;
    f32           end        = 10.0f;
    f32           total_time = 2.0f;

    poly5_generate_simple(start, end, total_time, &coeff);

    f32 pos_25 = poly5_eval_pos(&coeff, total_time * 0.25f);
    f32 pos_50 = poly5_eval_pos(&coeff, total_time * 0.5f);
    f32 pos_75 = poly5_eval_pos(&coeff, total_time * 0.75f);

    TEST_EXPECT_EQ_TRUE(pos_25 < pos_50);
    TEST_EXPECT_EQ_TRUE(pos_50 < pos_75);
}

TEST_CASE(test_poly5_smooth_transition)
{
    poly5_coeff_t coeff;
    f32           start      = 0.0f;
    f32           end        = 10.0f;
    f32           total_time = 2.0f;

    poly5_generate_simple(start, end, total_time, &coeff);

    f32 prev_pos = poly5_eval_pos(&coeff, 0.0f);
    for (f32 t = 0.1f; t <= total_time; t += 0.1f) {
        f32 pos = poly5_eval_pos(&coeff, t);
        TEST_EXPECT_EQ_TRUE(pos >= prev_pos);
        prev_pos = pos;
    }
}

TEST_CASE(test_poly5_zero_distance)
{
    poly5_coeff_t coeff;
    f32           total_time = 2.0f;

    poly5_generate_simple(5.0f, 5.0f, total_time, &coeff);

    f32 pos_start = poly5_eval_pos(&coeff, 0.0f);
    f32 pos_end   = poly5_eval_pos(&coeff, total_time);

    TEST_EXPECT_EQ_F32_E(5.0f, pos_start, EPSILON);
    TEST_EXPECT_EQ_F32_E(5.0f, pos_end, EPSILON);
}