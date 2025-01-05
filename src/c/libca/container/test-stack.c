#include <libca/container/stack.h>
#include <assert.h>
#include <stdio.h>
#include <libca/core/test.h>

TEST_CASE(stack_test1)
{
    // 初始化栈
    stack_t s;
    uint8_t buf[10];
    stack_init(&s, buf, 10);

    // 测试栈的初始化
    assert(s.top == buf);
    assert(s.bottom == buf);
    assert(s.size == 0);
    assert(s.capacity == 10);

    // 测试压栈
    assert(stack_push(&s, (void*)"Hello", 6) == true);
    assert(s.size == 6);
    assert(buf[0] == 'H');
    assert(buf[1] == 'e');
    assert(buf[2] == 'l');
    assert(buf[3] == 'l');
    assert(buf[4] == 'o');
    assert(buf[5] == '\0');

    // 测试弹栈
    assert(stack_pop(&s, 6) == true);
    assert(s.size == 0);

    // 测试获取栈顶数据
    uint8_t buf2[10];
    assert(stack_push(&s, (void*)"World", 6) == true);
    assert(stack_peek(&s, buf2, 6) == true);
    assert(s.size == 6);
    assert(buf[0] == 'W');
    assert(buf[1] == 'o');
    assert(buf[2] == 'r');
    assert(buf[3] == 'l');
    assert(buf[4] == 'd');
    assert(buf[5] == '\0');

    // 测试栈是否为空
    assert(stack_empty(&s) == false);

    // 测试栈是否已满
    stack_push(&s, (void*)"1234", 4);
    assert(stack_push(&s, (void*)"1234567890", 10) == false);
    assert(s.size == 10);
    assert(buf[9] == '4');

    printf("All tests pass");
}

// int main(int argc, char* argv[])
// {
//     test1();
//     return 0;
// }