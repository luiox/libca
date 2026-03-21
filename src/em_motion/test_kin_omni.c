/**
 * @file test_kin_omni.c
 * @brief Unit tests for omni wheel (3-wheel) kinematics
 */

#define _USE_MATH_DEFINES
#include "test.h"
#include "kin_omni.h"
#include <math.h>

#define WHEEL_RADIUS 0.05f
#define CHASSIS_RADIUS 0.2f
#define EPSILON 1e-5f

static omni3_param_t test_param = {.wheel_radius = WHEEL_RADIUS, .chassis_radius = CHASSIS_RADIUS};

TEST_CASE(test_omni3_ik_forward_x)
{
    f32 wheel_speed[3] = {0.0f};

    omni3_ik(1.0f, 0.0f, 0.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E(-0.5f / WHEEL_RADIUS, wheel_speed[0], EPSILON);
    TEST_EXPECT_EQ_F32_E(-0.5f / WHEEL_RADIUS, wheel_speed[1], EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f / WHEEL_RADIUS, wheel_speed[2], EPSILON);
}

TEST_CASE(test_omni3_ik_forward_y)
{
    f32 wheel_speed[3] = {0.0f};
    f32 sqrt3          = 1.7320508f;

    omni3_ik(0.0f, 1.0f, 0.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E((sqrt3 * 0.5f) / WHEEL_RADIUS, wheel_speed[0], EPSILON);
    TEST_EXPECT_EQ_F32_E(-(sqrt3 * 0.5f) / WHEEL_RADIUS, wheel_speed[1], EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, wheel_speed[2], EPSILON);
}

TEST_CASE(test_omni3_ik_rotation)
{
    f32 wheel_speed[3] = {0.0f};

    omni3_ik(0.0f, 0.0f, 1.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E(CHASSIS_RADIUS / WHEEL_RADIUS, wheel_speed[0], EPSILON);
    TEST_EXPECT_EQ_F32_E(CHASSIS_RADIUS / WHEEL_RADIUS, wheel_speed[1], EPSILON);
    TEST_EXPECT_EQ_F32_E(CHASSIS_RADIUS / WHEEL_RADIUS, wheel_speed[2], EPSILON);
}

TEST_CASE(test_omni3_ik_combined)
{
    f32 wheel_speed[3] = {0.0f};

    omni3_ik(1.0f, 1.0f, 1.0f, &test_param, wheel_speed);

    TEST_EXPECT_EQ_TRUE(wheel_speed[0] != 0.0f);
    TEST_EXPECT_EQ_TRUE(wheel_speed[1] != 0.0f);
    TEST_EXPECT_EQ_TRUE(wheel_speed[2] != 0.0f);
}

TEST_CASE(test_omni3_ik_null_pointers)
{
    f32 wheel_speed[3] = {0.0f};

    omni3_ik(1.0f, 0.0f, 0.0f, NULL, wheel_speed);
    omni3_ik(1.0f, 0.0f, 0.0f, &test_param, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_omni3_ik_invalid_wheel_radius)
{
    f32           wheel_speed[3] = {0.0f};
    omni3_param_t invalid_param  = {.wheel_radius = 0.0f, .chassis_radius = CHASSIS_RADIUS};

    omni3_ik(1.0f, 0.0f, 0.0f, &invalid_param, wheel_speed);

    TEST_EXPECT_EQ_F32_E(0.0f, wheel_speed[0], EPSILON);
}

TEST_CASE(test_omni3_fk_forward_x)
{
    f32 vx = 0.0f, vy = 0.0f, wz = 0.0f;
    f32 wheel_speed[3] = {-0.5f / WHEEL_RADIUS, -0.5f / WHEEL_RADIUS, 1.0f / WHEEL_RADIUS};

    omni3_fk(wheel_speed, &test_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(1.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, wz, EPSILON);
}

TEST_CASE(test_omni3_fk_forward_y)
{
    f32 vx = 0.0f, vy = 0.0f, wz = 0.0f;
    f32 sqrt3          = 1.7320508f;
    f32 wheel_speed[3] = {(sqrt3 * 0.5f) / WHEEL_RADIUS, -(sqrt3 * 0.5f) / WHEEL_RADIUS, 0.0f};

    omni3_fk(wheel_speed, &test_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(0.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, wz, EPSILON);
}

TEST_CASE(test_omni3_fk_rotation)
{
    f32 vx = 0.0f, vy = 0.0f, wz = 0.0f;
    f32 wheel_speed[3] = {CHASSIS_RADIUS / WHEEL_RADIUS,
                          CHASSIS_RADIUS / WHEEL_RADIUS,
                          CHASSIS_RADIUS / WHEEL_RADIUS};

    omni3_fk(wheel_speed, &test_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(0.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, wz, EPSILON);
}

TEST_CASE(test_omni3_fk_null_handling)
{
    f32 wheel_speed[3] = {1.0f, 1.0f, 1.0f};

    omni3_fk(wheel_speed, &test_param, NULL, NULL, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_omni3_fk_invalid_params)
{
    f32           vx = 1.0f, vy = 1.0f, wz = 1.0f;
    f32           wheel_speed[3] = {1.0f, 1.0f, 1.0f};
    omni3_param_t invalid_param  = {.wheel_radius = 0.0f, .chassis_radius = CHASSIS_RADIUS};

    omni3_fk(wheel_speed, &invalid_param, &vx, &vy, &wz);

    TEST_EXPECT_EQ_F32_E(0.0f, vx, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, vy, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, wz, EPSILON);
}

TEST_CASE(test_omni3_odometry_init)
{
    omni3_odometry_t odo = {1.0f, 2.0f, 3.0f};

    omni3_odometry_init(&odo);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.theta, EPSILON);
}

TEST_CASE(test_omni3_odometry_init_null)
{
    omni3_odometry_init(NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_omni3_odometry_update_forward)
{
    omni3_odometry_t odo;

    omni3_odometry_init(&odo);
    omni3_odometry_update(&odo, 1.0f, 0.0f, 0.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(1.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.theta, EPSILON);
}

TEST_CASE(test_omni3_odometry_update_strafe)
{
    omni3_odometry_t odo;

    omni3_odometry_init(&odo);
    omni3_odometry_update(&odo, 0.0f, 1.0f, 0.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.theta, EPSILON);
}

TEST_CASE(test_omni3_odometry_update_rotation)
{
    omni3_odometry_t odo;

    omni3_odometry_init(&odo);
    omni3_odometry_update(&odo, 0.0f, 0.0f, 1.0f, 1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, odo.y, EPSILON);
    TEST_EXPECT_EQ_F32_E(1.0f, odo.theta, EPSILON);
}

TEST_CASE(test_omni3_odometry_update_null_pointer)
{
    omni3_odometry_update(NULL, 1.0f, 0.0f, 0.0f, 1.0f);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_omni3_odometry_update_invalid_dt)
{
    omni3_odometry_t odo;

    omni3_odometry_init(&odo);
    omni3_odometry_update(&odo, 1.0f, 0.0f, 0.0f, 0.0f);
    omni3_odometry_update(&odo, 1.0f, 0.0f, 0.0f, -1.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, odo.x, EPSILON);
}

TEST_CASE(test_omni3_ik_fk_inverse)
{
    f32 vx_in = 1.5f, vy_in = 0.8f, wz_in = 0.5f;
    f32 wheel_speed[3] = {0.0f};
    f32 vx_out = 0.0f, vy_out = 0.0f, wz_out = 0.0f;

    omni3_ik(vx_in, vy_in, wz_in, &test_param, wheel_speed);
    omni3_fk(wheel_speed, &test_param, &vx_out, &vy_out, &wz_out);

    TEST_EXPECT_EQ_F32_E(vx_in, vx_out, EPSILON);
    TEST_EXPECT_EQ_F32_E(vy_in, vy_out, EPSILON);
    TEST_EXPECT_EQ_F32_E(wz_in, wz_out, EPSILON);
}