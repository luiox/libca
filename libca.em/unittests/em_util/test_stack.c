/* Auto-migrated from src/em_util/stack.c test blocks */
#include "stack.h"
#include <assert.h>
#include <string.h>


#include <em_test/test.h>

TEST_CASE(stack_basic)
{
    // 初始化栈
    stack_t s;
    uint8_t buf[10];
    stack_init(&s, buf, 10);

    // 测试栈的初始化
    TEST_ASSERT(s.top == buf);
    TEST_ASSERT(s.bottom == buf);
    TEST_ASSERT_EQUAL_INT(0, (int)s.size);
    TEST_ASSERT_EQUAL_INT(10, (int)s.capacity);

    // 测试压栈
    TEST_ASSERT(stack_push(&s, (void*)"Hello", 6) == true);
    TEST_ASSERT_EQUAL_INT(6, (int)s.size);
    TEST_ASSERT_EQUAL_INT('H', buf[0]);
    TEST_ASSERT_EQUAL_INT('e', buf[1]);
    TEST_ASSERT_EQUAL_INT('l', buf[2]);
    TEST_ASSERT_EQUAL_INT('l', buf[3]);
    TEST_ASSERT_EQUAL_INT('o', buf[4]);
    TEST_ASSERT_EQUAL_INT('\0', buf[5]);

    // 测试弹栈
    TEST_ASSERT(stack_pop(&s, 6) == true);
    TEST_ASSERT_EQUAL_INT(0, (int)s.size);

    // 测试获取栈顶数据
    uint8_t buf2[10];
    TEST_ASSERT(stack_push(&s, (void*)"World", 6) == true);
    TEST_ASSERT(stack_peek(&s, buf2, 6) == true);
    TEST_ASSERT_EQUAL_INT(6, (int)s.size);
    TEST_ASSERT_EQUAL_INT('W', buf[0]);
    TEST_ASSERT_EQUAL_INT('o', buf[1]);
    TEST_ASSERT_EQUAL_INT('r', buf[2]);
    TEST_ASSERT_EQUAL_INT('l', buf[3]);
    TEST_ASSERT_EQUAL_INT('d', buf[4]);
    TEST_ASSERT_EQUAL_INT('\0', buf[5]);

    // 测试栈是否为空
    TEST_ASSERT(stack_empty(&s) == false);

    // 测试栈是否已满
    stack_push(&s, (void*)"1234", 4);
    TEST_ASSERT(stack_push(&s, (void*)"1234567890", 10) == false);
    TEST_ASSERT_EQUAL_INT(10, (int)s.size);
    TEST_ASSERT_EQUAL_INT('4', buf[9]);
}
