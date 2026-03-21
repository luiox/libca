/**
 * @file test_trapezoidal.c
 * @brief Unit tests for trapezoidal trajectory planner
 */

#define _USE_MATH_DEFINES
#include "test.h"
#include "trapezoidal.h"
#include <math.h>

#define EPSILON 1e-4f

TEST_CASE(test_trapezoidal_init_trapezoidal_profile)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);

    TEST_EXPECT_EQ_TRUE(!plan.triangular);
    TEST_EXPECT_EQ_TRUE(plan.t_acc > 0.0f);
    TEST_EXPECT_EQ_TRUE(plan.t_cruise > 0.0f);
    TEST_EXPECT_EQ_TRUE(plan.t_dec > 0.0f);
    TEST_EXPECT_EQ_TRUE(plan.t_total > 0.0f);
}

TEST_CASE(test_trapezoidal_init_triangular_profile)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 1.0f, 10.0f, 1.0f);

    TEST_EXPECT_EQ_TRUE(plan.triangular);
    TEST_EXPECT_EQ_TRUE(plan.t_acc > 0.0f);
    TEST_EXPECT_EQ_F32_E(0.0f, plan.t_cruise, EPSILON);
    TEST_EXPECT_EQ_TRUE(plan.t_dec > 0.0f);
}

TEST_CASE(test_trapezoidal_init_null_pointer)
{
    trapezoidal_init(NULL, 0.0f, 10.0f, 2.0f, 1.0f);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_trapezoidal_init_zero_distance)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 5.0f, 5.0f, 2.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, plan.t_total, EPSILON);
}

TEST_CASE(test_trapezoidal_init_negative_params)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, -2.0f, -1.0f);

    TEST_EXPECT_EQ_TRUE(plan.cfg.max_vel > 0.0f);
    TEST_EXPECT_EQ_TRUE(plan.cfg.accel > 0.0f);
}

TEST_CASE(test_trapezoidal_init_backward_motion)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 10.0f, 0.0f, 2.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(-1.0f, plan.direction, EPSILON);
    TEST_EXPECT_EQ_TRUE(plan.t_total > 0.0f);
}

TEST_CASE(test_trapezoidal_update_at_start)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos = trapezoidal_update(&plan, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_trapezoidal_update_at_end)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos = trapezoidal_update(&plan, plan.t_total);

    TEST_EXPECT_EQ_F32_E(10.0f, pos, EPSILON);
}

TEST_CASE(test_trapezoidal_update_after_end)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos = trapezoidal_update(&plan, plan.t_total + 1.0f);

    TEST_EXPECT_EQ_F32_E(10.0f, pos, EPSILON);
}

TEST_CASE(test_trapezoidal_update_negative_time)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos = trapezoidal_update(&plan, -1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_trapezoidal_update_during_accel)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos      = trapezoidal_update(&plan, 0.5f);
    f32 expected = 0.5f * 1.0f * 0.5f * 0.5f;

    TEST_EXPECT_EQ_F32_E(expected, pos, EPSILON);
}

TEST_CASE(test_trapezoidal_update_during_cruise)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);
    f32 t = plan.t_acc + plan.t_cruise * 0.5f;

    f32 pos = trapezoidal_update(&plan, t);

    TEST_EXPECT_EQ_TRUE(pos > 0.0f);
    TEST_EXPECT_EQ_TRUE(pos < 10.0f);
}

TEST_CASE(test_trapezoidal_update_during_decel)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);
    f32 t = plan.t_acc + plan.t_cruise + plan.t_dec * 0.5f;

    f32 pos = trapezoidal_update(&plan, t);

    TEST_EXPECT_EQ_TRUE(pos > 0.0f);
    TEST_EXPECT_EQ_TRUE(pos < 10.0f);
}

TEST_CASE(test_trapezoidal_update_null_pointer)
{
    f32 pos = trapezoidal_update(NULL, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_trapezoidal_update_backward)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 10.0f, 0.0f, 2.0f, 1.0f);

    f32 pos_start = trapezoidal_update(&plan, 0.0f);
    f32 pos_end   = trapezoidal_update(&plan, plan.t_total);

    TEST_EXPECT_EQ_F32_E(10.0f, pos_start, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, pos_end, EPSILON);
}

TEST_CASE(test_trapezoidal_calc_basic)
{
    f32 pos_start = trapezoidal_calc(0.0f, 10.0f, 2.0f, 1.0f, 0.0f);
    f32 pos_end   = trapezoidal_calc(0.0f, 10.0f, 2.0f, 1.0f, 100.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos_start, EPSILON);
    TEST_EXPECT_EQ_F32_E(10.0f, pos_end, EPSILON);
}

TEST_CASE(test_trapezoidal_monotonic_increase)
{
    trapezoidal_t plan;
    f32           prev_pos;

    trapezoidal_init(&plan, 0.0f, 10.0f, 2.0f, 1.0f);

    prev_pos = trapezoidal_update(&plan, 0.0f);
    for (f32 t = 0.1f; t <= plan.t_total; t += 0.1f) {
        f32 pos = trapezoidal_update(&plan, t);
        TEST_EXPECT_EQ_TRUE(pos >= prev_pos);
        prev_pos = pos;
    }
}

TEST_CASE(test_trapezoidal_zero_vel_accel)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 10.0f, 0.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, plan.t_total, EPSILON);
}

TEST_CASE(test_trapezoidal_symmetric_triangular)
{
    trapezoidal_t plan;

    trapezoidal_init(&plan, 0.0f, 2.0f, 10.0f, 1.0f);

    f32 t_half        = plan.t_acc;
    f32 pos_half      = trapezoidal_update(&plan, t_half);
    f32 expected_half = 1.0f;

    TEST_EXPECT_EQ_F32_E(expected_half, pos_half, EPSILON);
}