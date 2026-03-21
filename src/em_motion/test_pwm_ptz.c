/**
 * @file test_pwm_ptz.c
 * @brief Unit tests for PWM PTZ control
 */

#define _USE_MATH_DEFINES
#include <em_test/test.h>
#include "pwm_ptz.h"
#include <math.h>

#define EPSILON 1e-5f

static i32  mock_init_called      = 0;
static i32  mock_enable_called    = 0;
static bool mock_enable_value     = false;
static i32  mock_set_angle_called = 0;
static f32  mock_last_pan         = 0.0f;
static f32  mock_last_tilt        = 0.0f;

static void mock_reset(void)
{
    mock_init_called      = 0;
    mock_enable_called    = 0;
    mock_enable_value     = false;
    mock_set_angle_called = 0;
    mock_last_pan         = 0.0f;
    mock_last_tilt        = 0.0f;
}

static i32 mock_init(void* ctx)
{
    (void)ctx;
    mock_init_called++;
    return 0;
}

static i32 mock_enable(void* ctx, bool enable)
{
    (void)ctx;
    mock_enable_called++;
    mock_enable_value = enable;
    return 0;
}

static i32 mock_set_angle(void* ctx, f32 pan, f32 tilt)
{
    (void)ctx;
    mock_set_angle_called++;
    mock_last_pan  = pan;
    mock_last_tilt = tilt;
    return 0;
}

static ptz_driver_t mock_driver = {
    .init = mock_init, .enable = mock_enable, .set_angle = mock_set_angle};

static pwm_ptz_limits_t test_limits = {.pan  = {.min_angle = -1.0f, .max_angle = 1.0f},
                                       .tilt = {.min_angle = -0.5f, .max_angle = 0.5f}};

TEST_CASE(test_pwm_ptz_init_basic)
{
    pwm_ptz_t ptz;
    mock_reset();

    pwm_ptz_init(&ptz, &mock_driver, NULL, &test_limits);

    TEST_EXPECT_EQ_I32(1, mock_init_called);
    TEST_EXPECT_EQ_F32_E(0.0f, ptz.target_pan, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.0f, ptz.target_tilt, EPSILON);
}

TEST_CASE(test_pwm_ptz_init_null_ptz)
{
    mock_reset();

    pwm_ptz_init(NULL, &mock_driver, NULL, &test_limits);

    TEST_EXPECT_EQ_I32(0, mock_init_called);
}

TEST_CASE(test_pwm_ptz_init_null_driver)
{
    pwm_ptz_t ptz;
    mock_reset();

    pwm_ptz_init(&ptz, NULL, NULL, &test_limits);

    TEST_EXPECT_EQ_TRUE(ptz.drv == NULL);
}

TEST_CASE(test_pwm_ptz_init_null_limits)
{
    pwm_ptz_t ptz;
    mock_reset();

    pwm_ptz_init(&ptz, &mock_driver, NULL, NULL);

    TEST_EXPECT_EQ_F32_E(-(f32)M_PI, ptz.limits.pan.min_angle, EPSILON);
    TEST_EXPECT_EQ_F32_E((f32)M_PI, ptz.limits.pan.max_angle, EPSILON);
}

TEST_CASE(test_pwm_ptz_enable_basic)
{
    pwm_ptz_t ptz;
    mock_reset();
    pwm_ptz_init(&ptz, &mock_driver, NULL, &test_limits);

    pwm_ptz_enable(&ptz, true);

    TEST_EXPECT_EQ_I32(1, mock_enable_called);
    TEST_EXPECT_EQ_TRUE(mock_enable_value);
}

TEST_CASE(test_pwm_ptz_enable_null_ptz)
{
    mock_reset();

    pwm_ptz_enable(NULL, true);

    TEST_EXPECT_EQ_I32(0, mock_enable_called);
}

TEST_CASE(test_pwm_ptz_enable_null_driver)
{
    pwm_ptz_t ptz;
    ptz.drv = NULL;
    mock_reset();

    pwm_ptz_enable(&ptz, true);

    TEST_EXPECT_EQ_I32(0, mock_enable_called);
}

TEST_CASE(test_pwm_ptz_set_target_basic)
{
    pwm_ptz_t ptz;
    mock_reset();
    pwm_ptz_init(&ptz, &mock_driver, NULL, &test_limits);

    pwm_ptz_set_target(&ptz, 0.5f, 0.25f);

    TEST_EXPECT_EQ_I32(1, mock_set_angle_called);
    TEST_EXPECT_EQ_F32_E(0.5f, mock_last_pan, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.25f, mock_last_tilt, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.5f, ptz.target_pan, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.25f, ptz.target_tilt, EPSILON);
}

TEST_CASE(test_pwm_ptz_set_target_null_ptz)
{
    mock_reset();

    pwm_ptz_set_target(NULL, 0.5f, 0.25f);

    TEST_EXPECT_EQ_I32(0, mock_set_angle_called);
}

TEST_CASE(test_pwm_ptz_set_target_clamp_high)
{
    pwm_ptz_t ptz;
    mock_reset();
    pwm_ptz_init(&ptz, &mock_driver, NULL, &test_limits);

    pwm_ptz_set_target(&ptz, 2.0f, 2.0f);

    TEST_EXPECT_EQ_F32_E(1.0f, ptz.target_pan, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.5f, ptz.target_tilt, EPSILON);
}

TEST_CASE(test_pwm_ptz_set_target_clamp_low)
{
    pwm_ptz_t ptz;
    mock_reset();
    pwm_ptz_init(&ptz, &mock_driver, NULL, &test_limits);

    pwm_ptz_set_target(&ptz, -2.0f, -2.0f);

    TEST_EXPECT_EQ_F32_E(-1.0f, ptz.target_pan, EPSILON);
    TEST_EXPECT_EQ_F32_E(-0.5f, ptz.target_tilt, EPSILON);
}

TEST_CASE(test_pwm_ptz_set_target_null_driver)
{
    pwm_ptz_t ptz;
    ptz.drv         = NULL;
    ptz.limits      = test_limits;
    ptz.target_pan  = 0.0f;
    ptz.target_tilt = 0.0f;
    mock_reset();

    pwm_ptz_set_target(&ptz, 0.5f, 0.25f);

    TEST_EXPECT_EQ_I32(0, mock_set_angle_called);
    TEST_EXPECT_EQ_F32_E(0.5f, ptz.target_pan, EPSILON);
    TEST_EXPECT_EQ_F32_E(0.25f, ptz.target_tilt, EPSILON);
}