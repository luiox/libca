/**
 * @file test_kin_ackermann.c
 * @brief Unit tests for Ackermann steering kinematics
 */

#define _USE_MATH_DEFINES
#include "test.h"
#include "kin_ackermann.h"
#include <math.h>

#define WHEEL_BASE 2.5f
#define WHEEL_TRACK 1.5f
#define EPSILON 1e-5f

static ackermann_param_t test_param = {.wheel_base = WHEEL_BASE, .wheel_track = WHEEL_TRACK};

TEST_CASE(test_ackermann_calc_steer_angle_straight)
{
    f32 angle = ackermann_calc_steer_angle(10.0f, 0.0f, WHEEL_BASE);

    TEST_EXPECT_EQ_F32_E(0.0f, angle, EPSILON);
}

TEST_CASE(test_ackermann_calc_steer_angle_zero_velocity)
{
    f32 angle = ackermann_calc_steer_angle(0.0f, 1.0f, WHEEL_BASE);

    TEST_EXPECT_EQ_F32_E(0.0f, angle, EPSILON);
}

TEST_CASE(test_ackermann_calc_steer_angle_turn)
{
    f32 v        = 10.0f;
    f32 w        = 0.5f;
    f32 expected = atanf((WHEEL_BASE * w) / v);

    f32 angle = ackermann_calc_steer_angle(v, w, WHEEL_BASE);

    TEST_EXPECT_EQ_F32_E(expected, angle, EPSILON);
}

TEST_CASE(test_ackermann_calc_steer_angle_invalid_wheel_base)
{
    f32 angle1 = ackermann_calc_steer_angle(10.0f, 1.0f, 0.0f);
    f32 angle2 = ackermann_calc_steer_angle(10.0f, 1.0f, -1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, angle1, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, angle2, EPSILON);
}

TEST_CASE(test_ackermann_split_steer_straight)
{
    f32 steer_left  = 0.0f;
    f32 steer_right = 0.0f;

    ackermann_split_steer(0.0f, &test_param, &steer_left, &steer_right);

    TEST_EXPECT_EQ_F32_E(0.0f, steer_left, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, steer_right, EPSILON);
}

TEST_CASE(test_ackermann_split_steer_left_turn)
{
    f32 steer_left   = 0.0f;
    f32 steer_right  = 0.0f;
    f32 steer_center = 0.1f;

    ackermann_split_steer(steer_center, &test_param, &steer_left, &steer_right);

    TEST_EXPECT_EQ_TRUE(steer_left > steer_center);
    TEST_EXPECT_EQ_TRUE(steer_right < steer_center);
    TEST_EXPECT_EQ_TRUE(steer_left > steer_right);
}

TEST_CASE(test_ackermann_split_steer_right_turn)
{
    f32 steer_left   = 0.0f;
    f32 steer_right  = 0.0f;
    f32 steer_center = -0.1f;

    ackermann_split_steer(steer_center, &test_param, &steer_left, &steer_right);

    TEST_EXPECT_EQ_TRUE(steer_left > steer_center);
    TEST_EXPECT_EQ_TRUE(steer_right < steer_center);
    TEST_EXPECT_EQ_TRUE(steer_right < steer_left);
}

TEST_CASE(test_ackermann_split_steer_null_pointers)
{
    f32 steer = 0.0f;

    ackermann_split_steer(0.1f, NULL, &steer, &steer);
    ackermann_split_steer(0.1f, &test_param, NULL, &steer);
    ackermann_split_steer(0.1f, &test_param, &steer, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_ackermann_split_steer_invalid_wheel_base)
{
    f32               steer_left    = 0.0f;
    f32               steer_right   = 0.0f;
    ackermann_param_t invalid_param = {.wheel_base = 0.0f, .wheel_track = WHEEL_TRACK};

    ackermann_split_steer(0.1f, &invalid_param, &steer_left, &steer_right);

    TEST_EXPECT_EQ_F32_E(0.0f, steer_left, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, steer_right, EPSILON);
}

TEST_CASE(test_ackermann_split_steer_invalid_wheel_track)
{
    f32               steer_left    = 0.0f;
    f32               steer_right   = 0.0f;
    ackermann_param_t invalid_param = {.wheel_base = WHEEL_BASE, .wheel_track = 0.0f};
    f32               steer_center  = 0.1f;

    ackermann_split_steer(steer_center, &invalid_param, &steer_left, &steer_right);

    TEST_EXPECT_EQ_F32_E(steer_center, steer_left, EPSILON);
    TEST_EXPECT_EQ_F32_E(steer_center, steer_right, EPSILON);
}

TEST_CASE(test_ackermann_geometry_symmetry)
{
    f32 steer_left_pos  = 0.0f;
    f32 steer_right_pos = 0.0f;
    f32 steer_left_neg  = 0.0f;
    f32 steer_right_neg = 0.0f;

    ackermann_split_steer(0.2f, &test_param, &steer_left_pos, &steer_right_pos);
    ackermann_split_steer(-0.2f, &test_param, &steer_left_neg, &steer_right_neg);

    TEST_EXPECT_EQ_F32_E(-steer_right_pos, steer_left_neg, EPSILON);
    TEST_EXPECT_EQ_F32_E(-steer_left_pos, steer_right_neg, EPSILON);
}