#include "scoroutine.h"
#include <string.h>

scoroutine_t g_scoroutines[SC_MAX_SIZE];
usize        g_scoroutine_count = 0;

#if TEST_ENABLE

#    include "../em_test/test.h"

// 模拟业务变量
static int g_test_counter = 0;

// 定义一个简单的测试协程
void test_task_yield(scoroutine_t* ctx)
{
    sc_begin(ctx);
    g_test_counter = 1;
    sc_yield();
    g_test_counter = 2;
    sc_yield();
    g_test_counter = 3;
    sc_end();
}

// 定义一个带延时的测试协程
void test_task_delay(scoroutine_t* ctx)
{
    sc_begin(ctx);
    g_test_counter = 10;
    sc_delay_ms(5);
    g_test_counter = 20;
    sc_end();
}

TEST_CASE(scoroutine_yield_logic)
{
    g_scoroutine_count = 0;
    g_test_counter     = 0;
    memset(g_scoroutines, 0, sizeof(g_scoroutines));

    sc_create_coroutine(test_task_yield);

    // 第一次运行：应该执行到第一个 yield
    g_scoroutines[0].cfunc(&g_scoroutines[0]);
    TEST_ASSERT_EQUAL_INT(1, g_test_counter);

    // 第二次运行：应该执行到第二个 yield
    g_scoroutines[0].cfunc(&g_scoroutines[0]);
    TEST_ASSERT_EQUAL_INT(2, g_test_counter);

    // 第三次运行：应该执行到结束
    g_scoroutines[0].cfunc(&g_scoroutines[0]);
    TEST_ASSERT_EQUAL_INT(3, g_test_counter);
}

TEST_CASE(scoroutine_delay_logic)
{
    g_scoroutine_count = 0;
    g_test_counter     = 0;
    memset(g_scoroutines, 0, sizeof(g_scoroutines));

    sc_create_coroutine(test_task_delay);

    // 第一次运行：设置 counter 为 10，并进入延时
    g_scoroutines[0].cfunc(&g_scoroutines[0]);
    TEST_ASSERT_EQUAL_INT(10, g_test_counter);
    TEST_ASSERT_EQUAL_INT(5, g_scoroutines[0].delay_tick);

    // 模拟 1ms 滴答
    tim_1ms_handler();
    TEST_ASSERT_EQUAL_INT(4, g_scoroutines[0].delay_tick);

    // 此时运行协程，因为 delay_tick > 0，逻辑上不应该继续执行
    if (g_scoroutines[0].delay_tick == 0) {
        g_scoroutines[0].cfunc(&g_scoroutines[0]);
    }
    TEST_ASSERT_EQUAL_INT(10, g_test_counter);

    // 模拟剩余 4ms
    for (int i = 0; i < 4; i++)
        tim_1ms_handler();
    TEST_ASSERT_EQUAL_INT(0, g_scoroutines[0].delay_tick);

    // 延时结束，再次运行
    if (g_scoroutines[0].delay_tick == 0) {
        g_scoroutines[0].cfunc(&g_scoroutines[0]);
    }
    TEST_ASSERT_EQUAL_INT(20, g_test_counter);
}

// 演示局部变量持久化 (通过上下文结构体)
typedef struct
{
    scoroutine_t base;
    int          counter;
} test_ctx_t;

void test_task_persistent_var(scoroutine_t* ctx)
{
    test_ctx_t* self = (test_ctx_t*)ctx;
    sc_begin(ctx);

    for (self->counter = 1; self->counter <= 3; self->counter++) {
        g_test_counter = self->counter;
        sc_yield();
    }

    sc_end();
}

TEST_CASE(scoroutine_persistence)
{
    static test_ctx_t my_ctx;
    memset(&my_ctx, 0, sizeof(my_ctx));
    g_test_counter = 0;

    // 运行 3 次循环
    test_task_persistent_var(&my_ctx.base);
    TEST_ASSERT_EQUAL_INT(1, g_test_counter);

    test_task_persistent_var(&my_ctx.base);
    TEST_ASSERT_EQUAL_INT(2, g_test_counter);

    test_task_persistent_var(&my_ctx.base);
    TEST_ASSERT_EQUAL_INT(3, g_test_counter);

    test_task_persistent_var(&my_ctx.base);
    TEST_ASSERT(sc_is_finished(&my_ctx.base));
}

// 演示嵌套协程
void sub_task(scoroutine_t* ctx)
{
    sc_begin(ctx);
    g_test_counter += 10;
    sc_yield();
    g_test_counter += 10;
    sc_end();
}

void parent_task(scoroutine_t* ctx)
{
    static scoroutine_t sub_ctx;
    sc_begin(ctx);

    g_test_counter = 1;
    sub_ctx.state  = SC_STATE_START;
    sc_await(sub_task, &sub_ctx);

    g_test_counter += 1;
    sc_end();
}

TEST_CASE(scoroutine_nesting)
{
    static scoroutine_t parent_ctx;
    memset(&parent_ctx, 0, sizeof(parent_ctx));
    g_test_counter = 0;

    // 1. Parent starts, calls sub_task, sub_task runs step 1 (counter: 0->1->11)
    parent_task(&parent_ctx);
    TEST_ASSERT_EQUAL_INT(11, g_test_counter);

    // 2. Parent continues, sub_task runs step 2 (counter: 11->21), then parent finishes (21->22)
    parent_task(&parent_ctx);
    TEST_ASSERT_EQUAL_INT(22, g_test_counter);
    TEST_ASSERT(sc_is_finished(&parent_ctx));
}

#if SC_HAS_LABEL_SUPPORT
// 演示嵌套 switch 冲突解决
void test_task_nested_switch(scoroutine_t* ctx)
{
    sc_begin(ctx);

    int val = 2;
    switch (val) {
    case 2:
        g_test_counter = 100;
        sc_yield();
        g_test_counter = 200;
        break;
    }

    sc_end();
}

TEST_CASE(scoroutine_nested_switch)
{
    static scoroutine_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    g_test_counter = 0;

    test_task_nested_switch(&ctx);
    TEST_ASSERT_EQUAL_INT(100, g_test_counter);

    test_task_nested_switch(&ctx);
    TEST_ASSERT_EQUAL_INT(200, g_test_counter);
}
#endif

// 演示：LED 闪烁任务 (使用上下文存储计数)
typedef struct
{
    scoroutine_t base;
    u32          count;
} led_ctx_t;

void led_blink_task(scoroutine_t* ctx)
{
    led_ctx_t* self = (led_ctx_t*)ctx;
    sc_begin(ctx);

    for (;;) {
        printf("  [LED] ON\n");
        for (self->count = 0; self->count < 2; self->count++) {
            sc_yield();
        }

        printf("  [LED] OFF\n");
        for (self->count = 0; self->count < 3; self->count++) {
            sc_yield();
        }
    }

    sc_end();
}

// 演示：按键检测任务 (多步处理)
void button_check_task(scoroutine_t* ctx)
{
    sc_begin(ctx);

    for (;;) {
        printf("  [Button] Checking...\n");
        // 模拟检测到按键
        printf("  [Button] PRESSED! Processing step 1...\n");
        sc_yield();

        printf("  [Button] Processing step 2...\n");
        sc_yield();

        printf("  [Button] Processing step 3... Done!\n");
        sc_yield();
    }

    sc_end();
}

TEST_CASE(scoroutine_demo_tasks)
{
    static led_ctx_t    led_ctx;
    static scoroutine_t btn_ctx;
    memset(&led_ctx, 0, sizeof(led_ctx));
    memset(&btn_ctx, 0, sizeof(btn_ctx));

    printf("\n--- Running Demo Tasks for 10 ticks ---\n");
    for (int i = 0; i < 10; i++) {
        printf("Tick %d:\n", i);
        led_blink_task(&led_ctx.base);
        button_check_task(&btn_ctx);
    }
}

#endif
