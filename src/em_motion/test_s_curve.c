/**
 * @file test_s_curve.c
 * @brief Unit tests for S-curve trajectory planner
 */

#define _USE_MATH_DEFINES
#include <em_test/test.h>
#include "s_curve.h"
#include <math.h>

#define EPSILON 1e-2f

static s_curve_config_t make_test_config(void)
{
    s_curve_config_t cfg = {
        .start = 0.0f, .end = 10.0f, .max_vel = 2.0f, .max_acc = 1.0f, .max_jerk = 2.0f};
    return cfg;
}

TEST_CASE(test_s_curve_init_basic)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    TEST_EXPECT_EQ_TRUE(plan.total_time > 0.0f);
}

TEST_CASE(test_s_curve_init_null_plan)
{
    s_curve_config_t cfg = make_test_config();

    s_curve_init(NULL, &cfg);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_s_curve_init_null_config)
{
    s_curve_t plan;

    s_curve_init(&plan, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_s_curve_update_at_start)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 pos = s_curve_update(&plan, 0.0f);

    TEST_EXPECT_EQ_F32_E(cfg.start, pos, EPSILON);
}

TEST_CASE(test_s_curve_update_at_end)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 pos = s_curve_update(&plan, plan.total_time);

    TEST_EXPECT_EQ_F32_E(cfg.end, pos, EPSILON);
}

TEST_CASE(test_s_curve_update_after_end)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 pos = s_curve_update(&plan, plan.total_time + 10.0f);

    TEST_EXPECT_EQ_F32_E(cfg.end, pos, EPSILON);
}

TEST_CASE(test_s_curve_update_negative_time)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 pos = s_curve_update(&plan, -1.0f);

    TEST_EXPECT_EQ_F32_E(cfg.start, pos, EPSILON);
}

TEST_CASE(test_s_curve_update_null_plan)
{
    f32 pos = s_curve_update(NULL, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_s_curve_update_during_motion)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 pos_start = s_curve_update(&plan, 0.0f);
    f32 pos_mid   = s_curve_update(&plan, plan.total_time * 0.5f);
    f32 pos_end   = s_curve_update(&plan, plan.total_time);

    TEST_EXPECT_EQ_TRUE(pos_mid > pos_start);
    TEST_EXPECT_EQ_TRUE(pos_mid < pos_end);
}

TEST_CASE(test_s_curve_backward_motion)
{
    s_curve_t        plan;
    s_curve_config_t cfg = {
        .start = 10.0f, .end = 0.0f, .max_vel = 2.0f, .max_acc = 1.0f, .max_jerk = 2.0f};

    s_curve_init(&plan, &cfg);

    f32 pos_start = s_curve_update(&plan, 0.0f);
    f32 pos_end   = s_curve_update(&plan, plan.total_time);

    TEST_EXPECT_EQ_F32_E(10.0f, pos_start, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, pos_end, EPSILON);
}

TEST_CASE(test_s_curve_zero_distance)
{
    s_curve_t        plan;
    s_curve_config_t cfg = {
        .start = 5.0f, .end = 5.0f, .max_vel = 2.0f, .max_acc = 1.0f, .max_jerk = 2.0f};

    s_curve_init(&plan, &cfg);

    f32 pos = s_curve_update(&plan, 0.5f);

    TEST_EXPECT_EQ_F32_E(5.0f, pos, EPSILON);
}

TEST_CASE(test_s_curve_get_velocity_at_start)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 vel = s_curve_get_velocity(&plan, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, vel, 0.1f);
}

TEST_CASE(test_s_curve_get_velocity_at_end)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 vel = s_curve_get_velocity(&plan, plan.total_time);

    TEST_EXPECT_EQ_F32_E(0.0f, vel, 0.1f);
}

TEST_CASE(test_s_curve_get_velocity_null_plan)
{
    f32 vel = s_curve_get_velocity(NULL, 0.5f);

    TEST_EXPECT_EQ_F32_E(0.0f, vel, EPSILON);
}

TEST_CASE(test_s_curve_get_acceleration_at_start)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 acc = s_curve_get_acceleration(&plan, 0.001f);

    TEST_EXPECT_EQ_TRUE(acc > 0.0f);
}

TEST_CASE(test_s_curve_get_acceleration_null_plan)
{
    f32 acc = s_curve_get_acceleration(NULL, 0.5f);

    TEST_EXPECT_EQ_F32_E(0.0f, acc, EPSILON);
}

TEST_CASE(test_s_curve_position_progression)
{
    s_curve_t        plan;
    s_curve_config_t cfg = make_test_config();

    s_curve_init(&plan, &cfg);

    f32 pos_start = s_curve_update(&plan, 0.0f);
    f32 pos_mid   = s_curve_update(&plan, plan.total_time * 0.5f);
    f32 pos_end   = s_curve_update(&plan, plan.total_time);

    TEST_EXPECT_EQ_TRUE(pos_start < pos_mid);
    TEST_EXPECT_EQ_TRUE(pos_mid < pos_end);
}

TEST_CASE(test_s_curve_short_distance)
{
    s_curve_t        plan;
    s_curve_config_t cfg = {
        .start = 0.0f, .end = 0.5f, .max_vel = 10.0f, .max_acc = 5.0f, .max_jerk = 10.0f};

    s_curve_init(&plan, &cfg);

    f32 pos_start = s_curve_update(&plan, 0.0f);
    f32 pos_end   = s_curve_update(&plan, plan.total_time);

    TEST_EXPECT_EQ_F32_E(0.0f, pos_start, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.5f, pos_end, EPSILON);
}