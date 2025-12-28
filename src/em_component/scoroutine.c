#include "scoroutine.h"

scoroutine_t g_scoroutines[SC_MAX_SIZE];
usize g_scoroutine_count = 0;

#if TEST_ENABLE

#include "../em_test/test.h"

// 模拟业务变量
static int g_test_counter = 0;

// 定义一个简单的测试协程
sc_begin(test_task_yield)
{
    g_test_counter = 1;
    sc_yield();
    g_test_counter = 2;
    sc_yield();
    g_test_counter = 3;
}
sc_end()

// 定义一个带延时的测试协程
sc_begin(test_task_delay)
{
    g_test_counter = 10;
    sc_delay_ms(5);
    g_test_counter = 20;
}
sc_end()

TEST_CASE(scoroutine_yield_logic)
{
    g_scoroutine_count = 0;
    g_test_counter = 0;
    memset(g_scoroutines, 0, sizeof(g_scoroutines));

    sc_create_coroutine(test_task_yield);
    
    // 第一次运行：应该执行到第一个 yield
    g_scoroutines[0].cfunc(0);
    TEST_ASSERT_EQUAL_INT(1, g_test_counter);
    
    // 第二次运行：应该执行到第二个 yield
    g_scoroutines[0].cfunc(0);
    TEST_ASSERT_EQUAL_INT(2, g_test_counter);
    
    // 第三次运行：应该执行到结束
    g_scoroutines[0].cfunc(0);
    TEST_ASSERT_EQUAL_INT(3, g_test_counter);
}

TEST_CASE(scoroutine_delay_logic)
{
    g_scoroutine_count = 0;
    g_test_counter = 0;
    memset(g_scoroutines, 0, sizeof(g_scoroutines));

    sc_create_coroutine(test_task_delay);

    // 第一次运行：设置 counter 为 10，并进入延时
    g_scoroutines[0].cfunc(0);
    TEST_ASSERT_EQUAL_INT(10, g_test_counter);
    TEST_ASSERT_EQUAL_INT(5, g_scoroutines[0].delay_tick);

    // 模拟 1ms 滴答
    tim_1ms_handler();
    TEST_ASSERT_EQUAL_INT(4, g_scoroutines[0].delay_tick);

    // 此时运行协程，因为 delay_tick > 0，逻辑上不应该继续执行
    if (g_scoroutines[0].delay_tick == 0) {
        g_scoroutines[0].cfunc(0);
    }
    TEST_ASSERT_EQUAL_INT(10, g_test_counter);

    // 模拟剩余 4ms
    for(int i=0; i<4; i++) tim_1ms_handler();
    TEST_ASSERT_EQUAL_INT(0, g_scoroutines[0].delay_tick);

    // 延时结束，再次运行
    if (g_scoroutines[0].delay_tick == 0) {
        g_scoroutines[0].cfunc(0);
    }
    TEST_ASSERT_EQUAL_INT(20, g_test_counter);
}

// 演示缺陷：局部变量丢失
sc_begin(test_task_local_var_fail)
{
    int local_val = 100;
    sc_yield();
    // 这里的 local_val 理论上已经不是 100 了
    g_test_counter = local_val; 
}
sc_end()

TEST_CASE(scoroutine_local_var_limitation)
{
    g_scoroutine_count = 0;
    g_test_counter = 0;
    memset(g_scoroutines, 0, sizeof(g_scoroutines));

    sc_create_coroutine(test_task_local_var_fail);

    g_scoroutines[0].cfunc(0); 
    g_scoroutines[0].cfunc(0); 
}

#endif
