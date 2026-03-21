/**
 * @file test_kin_mecanum.c
 * @brief Unit tests for mecanum wheel kinematics
 */

#define _USE_MATH_DEFINES
#include <em_test/test.h>
#include "kin_mecanum.h"
#include <math.h>

#define WHEEL_RADIUS 0.05f
#define HALF_LENGTH 0.2f
#define HALF_WIDTH 0.15f
#define EPSILON 1e-5f

static mecanum_param_t test_param = {
    .wheel_radius = WHEEL_RADIUS, .half_length = HALF_LENGTH, .half_width = HALF_WIDTH};

TEST_CASE(test_mecanum_ik_forward_x)
{
    f32 wheel_speed[4] = {0.0f};

    mecanum_ik(1.0f, 0.0f, 0.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E(1.0f / WHEEL_RADIUS, wheel_speed[0], EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f / WHEEL_RADIUS, wheel_speed[1], EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f / WHEEL_RADIUS, wheel_speed[2], EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f / WHEEL_RADIUS, wheel_speed[3], EPSILON);
}

TEST_CASE(test_mecanum_ik_strafe_y)
{
    f32 wheel_speed[4] = {0.0f};

    mecanum_ik(0.0f, 1.0f, 0.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E(-1.0f / WHEEL_RADIUS, wheel_speed[0], EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f / WHEEL_RADIUS, wheel_speed[1], EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f / WHEEL_RADIUS, wheel_speed[2], EPSILON);
    TEST_EXPECT_EQ_F32_E(-1.0f / WHEEL_RADIUS, wheel_speed[3], EPSILON);
}

TEST_CASE(test_mecanum_ik_rotation)
{
    f32 wheel_speed[4] = {0.0f};
    f32 k              = HALF_LENGTH + HALF_WIDTH;

    mecanum_ik(0.0f, 0.0f, 1.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E(-k / WHEEL_RADIUS, wheel_speed[0], EPSILON);
    TEST_EXPECT_EQ_F32_E(k / WHEEL_RADIUS, wheel_speed[1], EPSILON);
    TEST_EXPECT_EQ_F32_E(-k / WHEEL_RADIUS, wheel_speed[2], EPSILON);
    TEST_EXPECT_EQ_F32_E(k / WHEEL_RADIUS, wheel_speed[3], EPSILON);
}

TEST_CASE(test_mecanum_ik_combined)
{
    f32 wheel_speed[4] = {0.0f};

    mecanum_ik(1.0f, 1.0f, 1.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_TRUE(wheel_speed[0] != 0.0f);
    TEST_EXPECT_EQ_TRUE(wheel_speed[1] != 0.0f);
    TEST_EXPECT_EQ_TRUE(wheel_speed[2] != 0.0f);
    TEST_EXPECT_EQ_TRUE(wheel_speed[3] != 0.0f);
}

TEST_CASE(test_mecanum_ik_null_pointers)
{
    f32 wheel_speed[4] = {0.0f};

    mecanum_ik(1.0f, 0.0f, 0.0f, NULL, wheel_speed);
    mecanum_ik(1.0f, 0.0f, 0.0f, &test_param, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_mecanum_ik_invalid_wheel_radius)
{
    f32             wheel_speed[4] = {0.0f};
    mecanum_param_t invalid_param  = {
         .wheel_radius = 0.0f, .half_length = HALF_LENGTH, .half_width = HALF_WIDTH};

    mecanum_ik(1.0f, 0.0f, 0.0f, &invalid_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E(0.0f, wheel_speed[0], EPSILON);
}

TEST_CASE(test_mecanum_fk_forward_x)
{
    f32 vx = 0.0f, vy = 0.0f, wz = 0.0f;
    f32 wheel_speed[4] = {
        1.0f / WHEEL_RADIUS, 1.0f / WHEEL_RADIUS, 1.0f / WHEEL_RADIUS, 1.0f / WHEEL_RADIUS};

    mecanum_fk(wheel_speed, &test_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(1.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, wz, EPSILON);
}

TEST_CASE(test_mecanum_fk_strafe_y)
{
    f32 vx = 0.0f, vy = 0.0f, wz = 0.0f;
    f32 wheel_speed[4] = {
        -1.0f / WHEEL_RADIUS, 1.0f / WHEEL_RADIUS, 1.0f / WHEEL_RADIUS, -1.0f / WHEEL_RADIUS};

    mecanum_fk(wheel_speed, &test_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(0.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, wz, EPSILON);
}

TEST_CASE(test_mecanum_fk_rotation)
{
    f32 vx = 0.0f, vy = 0.0f, wz = 0.0f;
    f32 k              = HALF_LENGTH + HALF_WIDTH;
    f32 wheel_speed[4] = {-k / WHEEL_RADIUS, k / WHEEL_RADIUS, -k / WHEEL_RADIUS, k / WHEEL_RADIUS};

    mecanum_fk(wheel_speed, &test_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(0.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, wz, EPSILON);
}

TEST_CASE(test_mecanum_fk_null_handling)
{
    f32 wheel_speed[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    mecanum_fk(wheel_speed, &test_param, NULL, NULL, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_mecanum_fk_invalid_params)
{
    f32             vx = 1.0f, vy = 1.0f, wz = 1.0f;
    f32             wheel_speed[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    mecanum_param_t invalid_param  = {
         .wheel_radius = 0.0f, .half_length = HALF_LENGTH, .half_width = HALF_WIDTH};

    mecanum_fk(wheel_speed, &invalid_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(0.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, wz, EPSILON);
}

TEST_CASE(test_mecanum_odometry_init)
{
    mecanum_odometry_t odo = {1.0f, 2.0f, 3.0f};

    mecanum_odometry_init(&odo);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.theta, EPSILON);
}

TEST_CASE(test_mecanum_odometry_init_null)
{
    mecanum_odometry_init(NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_mecanum_odometry_update_forward)
{
    mecanum_odometry_t odo;

    mecanum_odometry_init(&odo);
    mecanum_odometry_update(&odo, 1.0f, 0.0f, 0.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(1.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.theta, EPSILON);
}

TEST_CASE(test_mecanum_odometry_update_strafe)
{
    mecanum_odometry_t odo;

    mecanum_odometry_init(&odo);
    mecanum_odometry_update(&odo, 0.0f, 1.0f, 0.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.theta, EPSILON);
}

TEST_CASE(test_mecanum_odometry_update_rotation)
{
    mecanum_odometry_t odo;

    mecanum_odometry_init(&odo);
    mecanum_odometry_update(&odo, 0.0f, 0.0f, 1.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, odo.theta, EPSILON);
}

TEST_CASE(test_mecanum_odometry_update_null_pointer)
{
    mecanum_odometry_update(NULL, 1.0f, 0.0f, 0.0f, 1.0f);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_mecanum_odometry_update_invalid_dt)
{
    mecanum_odometry_t odo;

    mecanum_odometry_init(&odo);
    mecanum_odometry_update(&odo, 1.0f, 0.0f, 0.0f, 0.0f);
    mecanum_odometry_update(&odo, 1.0f, 0.0f, 0.0f, -1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
}

TEST_CASE(test_mecanum_ik_fk_inverse)
{
    f32 vx_in = 1.5f, vy_in = 0.8f, wz_in = 0.5f;
    f32 wheel_speed[4] = {0.0f};
    f32 vx_out = 0.0f, vy_out = 0.0f, wz_out = 0.0f;

    mecanum_ik(vx_in, vy_in, wz_in, &test_param, wheel_speed);
    mecanum_fk(wheel_speed, &test_param, &vx_out, &vy_out, &wz_out);

    TEST_EXPECT_EQ_F32_E(vx_in, vx_out, EPSILON);
    TEST_EXPECT_EQ_F32_E(vy_in, vy_out, EPSILON);
    TEST_EXPECT_EQ_F32_E(wz_in, wz_out, EPSILON);
}