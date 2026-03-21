/**
 * @file test_kin_diff.c
 * @brief Unit tests for differential drive kinematics
 */

#define _USE_MATH_DEFINES
#include <em_test/test.h>
#include "kin_diff.h"
#include <math.h>

#define WHEEL_BASE 0.5f
#define EPSILON 1e-5f

TEST_CASE(test_diff_drive_ik_forward)
{
    f32 left  = 0.0f;
    f32 right = 0.0f;

    diff_drive_ik(1.0f, 0.0f, WHEEL_BASE, &left, &right);

    TEST_EXPECT_EQ_F32_E(1.0f, left, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, right, EPSILON);
}

TEST_CASE(test_diff_drive_ik_rotation)
{
    f32 left  = 0.0f;
    f32 right = 0.0f;

    diff_drive_ik(0.0f, 2.0f, WHEEL_BASE, &left, &right);

    TEST_EXPECT_EQ_F32_E(-0.5f, left, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.5f, right, EPSILON);
}

TEST_CASE(test_diff_drive_ik_combined)
{
    f32 left  = 0.0f;
    f32 right = 0.0f;

    diff_drive_ik(1.0f, 2.0f, WHEEL_BASE, &left, &right);

    TEST_EXPECT_EQ_F32_E(0.5f, left, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.5f, right, EPSILON);
}

TEST_CASE(test_diff_drive_ik_null_pointers)
{
    f32 left = 0.0f;

    diff_drive_ik(1.0f, 0.0f, WHEEL_BASE, NULL, &left);
    diff_drive_ik(1.0f, 0.0f, WHEEL_BASE, &left, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_diff_drive_ik_invalid_wheel_base)
{
    f32 left  = 0.0f;
    f32 right = 0.0f;

    diff_drive_ik(1.0f, 0.0f, 0.0f, &left, &right);
    diff_drive_ik(1.0f, 0.0f, -1.0f, &left, &right);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_diff_drive_fk_forward)
{
    f32 v = 0.0f;
    f32 w = 0.0f;

    diff_drive_fk(1.0f, 1.0f, WHEEL_BASE, &v, &w);

    TEST_EXPECT_EQ_F32_E(1.0f, v, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, w, EPSILON);
}

TEST_CASE(test_diff_drive_fk_rotation)
{
    f32 v = 0.0f;
    f32 w = 0.0f;

    diff_drive_fk(-0.5f, 0.5f, WHEEL_BASE, &v, &w);

    TEST_EXPECT_EQ_F32_E(0.0f, v, EPSILON);
    TEST_EXPECT_EQ_F32_E(2.0f, w, EPSILON);
}

TEST_CASE(test_diff_drive_fk_combined)
{
    f32 v = 0.0f;
    f32 w = 0.0f;

    diff_drive_fk(0.5f, 1.5f, WHEEL_BASE, &v, &w);

    TEST_EXPECT_EQ_F32_E(1.0f, v, EPSILON);
    TEST_EXPECT_EQ_F32_E(2.0f, w, EPSILON);
}

TEST_CASE(test_diff_drive_fk_null_pointers)
{
    f32 v = 0.0f;

    diff_drive_fk(1.0f, 1.0f, WHEEL_BASE, NULL, &v);
    diff_drive_fk(1.0f, 1.0f, WHEEL_BASE, &v, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_diff_drive_fk_invalid_wheel_base)
{
    f32 v = 0.0f;
    f32 w = 0.0f;

    diff_drive_fk(1.0f, 1.0f, 0.0f, &v, &w);
    diff_drive_fk(1.0f, 1.0f, -1.0f, &v, &w);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_diff_drive_odometry_forward)
{
    pose2d_t pose = {0.0f, 0.0f, 0.0f};

    diff_drive_odometry(1.0f, 1.0f, WHEEL_BASE, 1.0f, &pose);

    TEST_EXPECT_EQ_F32_E(1.0f, pose.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, pose.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, pose.theta, EPSILON);
}

TEST_CASE(test_diff_drive_odometry_rotation)
{
    pose2d_t pose = {0.0f, 0.0f, 0.0f};

    diff_drive_odometry(-0.5f, 0.5f, WHEEL_BASE, 1.0f, &pose);

    TEST_EXPECT_EQ_F32_E(0.0f, pose.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, pose.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(2.0f, pose.theta, EPSILON);
}

TEST_CASE(test_diff_drive_odometry_diagonal)
{
    pose2d_t pose         = {0.0f, 0.0f, 0.0f};
    f32      expected_pos = cosf((f32)M_PI / 4.0f);

    pose.theta = (f32)M_PI / 4.0f;
    diff_drive_odometry(1.0f, 1.0f, WHEEL_BASE, 1.0f, &pose);

    TEST_EXPECT_EQ_F32_E(expected_pos, pose.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(expected_pos, pose.y, EPSILON);
}

TEST_CASE(test_diff_drive_odometry_null_pointer)
{
    diff_drive_odometry(1.0f, 1.0f, WHEEL_BASE, 1.0f, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_diff_drive_odometry_invalid_params)
{
    pose2d_t pose = {0.0f, 0.0f, 0.0f};

    diff_drive_odometry(1.0f, 1.0f, 0.0f, 1.0f, &pose);
    diff_drive_odometry(1.0f, 1.0f, WHEEL_BASE, 0.0f, &pose);
    diff_drive_odometry(1.0f, 1.0f, WHEEL_BASE, -1.0f, &pose);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_ik_fk_inverse)
{
    f32 v_in  = 2.0f;
    f32 w_in  = 1.0f;
    f32 left  = 0.0f;
    f32 right = 0.0f;

    diff_drive_ik(v_in, w_in, WHEEL_BASE, &left, &right);

    f32 v_out = 0.0f;
    f32 w_out = 0.0f;
    diff_drive_fk(left, right, WHEEL_BASE, &v_out, &w_out);

    TEST_EXPECT_EQ_F32_E(v_in, v_out, EPSILON);
    TEST_EXPECT_EQ_F32_E(w_in, w_out, EPSILON);
}