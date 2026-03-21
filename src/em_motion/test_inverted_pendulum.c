/**
 * @file test_inverted_pendulum.c
 * @brief Unit tests for inverted pendulum control
 */

#define _USE_MATH_DEFINES
#include <em_test/test.h>
#include "inverted_pendulum.h"
#include <math.h>

#define EPSILON 1e-5f

TEST_CASE(test_inverted_pendulum_pd_at_target)
{
    f32 torque = inverted_pendulum_pd(0.0f, 0.0f, 0.0f, 1.0f, 0.5f);

    TEST_EXPECT_EQ_F32_E(0.0f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_pd_positive_error)
{
    f32 kp     = 2.0f;
    f32 kd     = 0.0f;
    f32 torque = inverted_pendulum_pd(0.0f, 0.0f, 0.1f, kp, kd);

    TEST_EXPECT_EQ_F32_E(kp * 0.1f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_pd_negative_error)
{
    f32 kp     = 2.0f;
    f32 kd     = 0.0f;
    f32 torque = inverted_pendulum_pd(0.1f, 0.0f, 0.0f, kp, kd);

    TEST_EXPECT_EQ_F32_E(-kp * 0.1f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_pd_damping)
{
    f32 kp     = 0.0f;
    f32 kd     = 1.0f;
    f32 torque = inverted_pendulum_pd(0.0f, 0.5f, 0.0f, kp, kd);

    TEST_EXPECT_EQ_F32_E(-0.5f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_pd_combined)
{
    f32 kp     = 2.0f;
    f32 kd     = 1.0f;
    f32 torque = inverted_pendulum_pd(0.1f, 0.2f, 0.0f, kp, kd);

    TEST_EXPECT_EQ_F32_E(-0.2f - 0.2f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_gravity_comp_upright)
{
    f32 mass    = 1.0f;
    f32 length  = 0.5f;
    f32 gravity = 9.8f;

    f32 torque = inverted_pendulum_gravity_comp(0.0f, mass, length, gravity);

    TEST_EXPECT_EQ_F32_E(0.0f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_gravity_comp_horizontal)
{
    f32 mass     = 1.0f;
    f32 length   = 0.5f;
    f32 gravity  = 9.8f;
    f32 expected = mass * gravity * length * 1.0f;

    f32 torque = inverted_pendulum_gravity_comp((f32)M_PI / 2.0f, mass, length, gravity);

    TEST_EXPECT_EQ_F32_E(expected, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_gravity_comp_negative_angle)
{
    f32 mass    = 1.0f;
    f32 length  = 0.5f;
    f32 gravity = 9.8f;

    f32 torque_pos = inverted_pendulum_gravity_comp(0.5f, mass, length, gravity);
    f32 torque_neg = inverted_pendulum_gravity_comp(-0.5f, mass, length, gravity);

    TEST_EXPECT_EQ_TRUE(torque_pos > 0.0f);
    TEST_EXPECT_EQ_TRUE(torque_neg < 0.0f);
}

TEST_CASE(test_inverted_pendulum_control_null_config)
{
    f32 torque = inverted_pendulum_control(0.0f, 0.0f, 0.0f, NULL);

    TEST_EXPECT_EQ_F32_E(0.0f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_control_basic)
{
    inverted_pendulum_cfg_t cfg = {
        .kp = 2.0f, .kd = 0.5f, .mass = 1.0f, .length = 0.5f, .gravity = 9.8f};

    f32 torque = inverted_pendulum_control(0.1f, 0.0f, 0.0f, &cfg);

    TEST_EXPECT_EQ_TRUE(torque != 0.0f);
}

TEST_CASE(test_inverted_pendulum_control_at_target)
{
    inverted_pendulum_cfg_t cfg = {
        .kp = 2.0f, .kd = 0.5f, .mass = 1.0f, .length = 0.5f, .gravity = 9.8f};

    f32 torque = inverted_pendulum_control(0.0f, 0.0f, 0.0f, &cfg);

    TEST_EXPECT_EQ_F32_E(0.0f, torque, EPSILON);
}

TEST_CASE(test_inverted_pendulum_control_with_angular_velocity)
{
    inverted_pendulum_cfg_t cfg = {
        .kp = 2.0f, .kd = 1.0f, .mass = 1.0f, .length = 0.5f, .gravity = 9.8f};

    f32 torque = inverted_pendulum_control(0.0f, 0.5f, 0.0f, &cfg);

    TEST_EXPECT_EQ_F32_E(-0.5f, torque, EPSILON);
}