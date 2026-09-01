/* Auto-migrated from src/em_util/pid.c test blocks */
#include "pid.h"


#include <em_test/test.h>

TEST_CASE(pid_position_basic)
{
    pid_position_t pid;
    pid_position_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);   // 目标10
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};

    // 简单验证第一个计算结果
    // error = 10 - 0 = 10
    // sum_error = 10
    // d_error = 10 - 0 = 10
    // output = 1.0*10 + 0.1*10 + 0.05*10 = 10 + 1 + 0.5 = 11.5
    float u = pid_position_calculate(&pid, values[0]);
    TEST_ASSERT_EQUAL_FLOAT(11.5f, u);
}

TEST_CASE(pid_incremental_basic)
{
    pid_incremental_t pid;
    pid_incremental_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};

    // 简单验证第一个增量结果
    // error = 10 - 0 = 10
    // last_error = 0, prev_error = 0
    // du = 1.0*(10-0) + 0.1*10 + 0.05*(10 - 2*0 + 0) = 10 + 1 + 0.5 = 11.5
    float du = pid_incremental_calculate(&pid, values[0]);
    TEST_ASSERT_EQUAL_FLOAT(11.5f, du);
}
