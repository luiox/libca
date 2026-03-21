/**
 * @file test_model_trajectory.c
 * @brief Unit tests for model trajectory wrapper
 */

#define _USE_MATH_DEFINES
#include "test.h"
#include "model_trajectory.h"
#include <math.h>

#define EPSILON 1e-4f

TEST_CASE(test_model_trajectory_init_trapezoidal)
{
    model_trajectory_t traj;

    model_trajectory_init_trapezoidal(&traj, 0.0f, 10.0f, 2.0f, 1.0f);

    TEST_EXPECT_EQ_I32(TRAJECTORY_TYPE_TRAPEZOIDAL, traj.type);
}

TEST_CASE(test_model_trajectory_init_trapezoidal_null)
{
    model_trajectory_init_trapezoidal(NULL, 0.0f, 10.0f, 2.0f, 1.0f);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_model_trajectory_init_s_curve)
{
    model_trajectory_t traj;
    s_curve_config_t   cfg = {
          .start = 0.0f, .end = 10.0f, .max_vel = 2.0f, .max_acc = 1.0f, .max_jerk = 2.0f};

    model_trajectory_init_s_curve(&traj, &cfg);

    TEST_EXPECT_EQ_I32(TRAJECTORY_TYPE_S_CURVE, traj.type);
}

TEST_CASE(test_model_trajectory_init_s_curve_null_traj)
{
    s_curve_config_t cfg = {
        .start = 0.0f, .end = 10.0f, .max_vel = 2.0f, .max_acc = 1.0f, .max_jerk = 2.0f};

    model_trajectory_init_s_curve(NULL, &cfg);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_model_trajectory_init_s_curve_null_config)
{
    model_trajectory_t traj;

    model_trajectory_init_s_curve(&traj, NULL);

    TEST_EXPECT_EQ_TRUE(true);
}

TEST_CASE(test_model_trajectory_eval_trapezoidal_at_start)
{
    model_trajectory_t traj;

    model_trajectory_init_trapezoidal(&traj, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos = model_trajectory_eval(&traj, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_model_trajectory_eval_trapezoidal_at_end)
{
    model_trajectory_t traj;

    model_trajectory_init_trapezoidal(&traj, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos = model_trajectory_eval(&traj, traj.trap.t_total);

    TEST_EXPECT_EQ_F32_E(10.0f, pos, EPSILON);
}

TEST_CASE(test_model_trajectory_eval_s_curve_at_start)
{
    model_trajectory_t traj;
    s_curve_config_t   cfg = {
          .start = 0.0f, .end = 10.0f, .max_vel = 2.0f, .max_acc = 1.0f, .max_jerk = 2.0f};

    model_trajectory_init_s_curve(&traj, &cfg);

    f32 pos = model_trajectory_eval(&traj, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_model_trajectory_eval_s_curve_at_end)
{
    model_trajectory_t traj;
    s_curve_config_t   cfg = {
          .start = 0.0f, .end = 10.0f, .max_vel = 2.0f, .max_acc = 1.0f, .max_jerk = 2.0f};

    model_trajectory_init_s_curve(&traj, &cfg);

    f32 pos = model_trajectory_eval(&traj, traj.s_curve.total_time);

    TEST_EXPECT_EQ_F32_E(10.0f, pos, 0.01f);
}

TEST_CASE(test_model_trajectory_eval_null)
{
    f32 pos = model_trajectory_eval(NULL, 0.0f);

    TEST_EXPECT_EQ_F32_E(0.0f, pos, EPSILON);
}

TEST_CASE(test_model_trajectory_eval_during_motion)
{
    model_trajectory_t traj;

    model_trajectory_init_trapezoidal(&traj, 0.0f, 10.0f, 2.0f, 1.0f);

    f32 pos_start = model_trajectory_eval(&traj, 0.0f);
    f32 pos_mid   = model_trajectory_eval(&traj, traj.trap.t_total * 0.5f);
    f32 pos_end   = model_trajectory_eval(&traj, traj.trap.t_total);

    TEST_EXPECT_EQ_TRUE(pos_mid > pos_start);
    TEST_EXPECT_EQ_TRUE(pos_mid < pos_end);
}