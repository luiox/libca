/* Auto-migrated from src/em_base/debug.c test blocks */
#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

#include <em_test/test.h>
#include <string.h>

// 测试用的捕获缓冲区
static char test_last_msg[CA_PRINT_BUFFER_SIZE];

static void test_hw_puts_cb(const char* s)
{
    // 拷贝到捕获缓冲区，并打印到控制台
    strncpy(test_last_msg, s, sizeof(test_last_msg) - 1);
    test_last_msg[sizeof(test_last_msg) - 1] = '\0';
    printf("HW输出: %s\n", s);
}

// 测试 debug_puts 与 debug_printf
TEST_CASE(debug_puts_and_printf)
{
    memset(test_last_msg, 0, sizeof(test_last_msg));

    debug_init(test_hw_puts_cb);

    debug_puts("abc");
    TEST_ASSERT_EQUAL_STRING("abc", test_last_msg);

    debug_printf("hello %d", 123);
    TEST_ASSERT_EQUAL_STRING("hello 123", test_last_msg);
}

TEST_CASE(test_param_check_macro)
{
#if CA_USE_PARAM_CHECK
    memset(test_last_msg, 0, sizeof(test_last_msg));
    debug_init(test_hw_puts_cb);

    // 这应该触发失败消息
    param_check(0);
    // 检查输出是否包含 "param check failure"
    // 由于 strstr 是标准库函数，我们可以直接使用。test.h 可能包含了 string.h 或者我们自己包含了。
    // 是的，debug.c 在 #if TEST_ENABLE 内部包含了 #include <string.h>
    TEST_ASSERT_NOT_NULL(strstr(test_last_msg, "param check failure"));
    
    // 检查正向情况（不应该打印任何内容）
    memset(test_last_msg, 0, sizeof(test_last_msg));
    param_check(1);
    TEST_ASSERT_EQUAL_STRING("", test_last_msg);
#endif
}

