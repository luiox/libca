/* Auto-migrated from src/em_platform/async.c test blocks */
#include "async.h"

#include <em_test/test.h>

static int g_test_counter = 0;

static void test_task_add(void *arg) {
    int val = (int)(intptr_t)arg;
    g_test_counter += val;
}

TEST_CASE(async_basic_functionality) {
    async_t queue;
    task_item_t buffer[4]; // Size 4
    
    // 1. Test Init
    TEST_ASSERT_TRUE(async_init(&queue, buffer, 4));
    TEST_ASSERT_FALSE(async_init(&queue, buffer, 3)); // Not power of 2
    
    // 2. Test Submit & Poll
    g_test_counter = 0;
    TEST_ASSERT_TRUE(async_submit(&queue, test_task_add, (void*)(intptr_t)10));
    TEST_ASSERT_TRUE(async_submit(&queue, test_task_add, (void*)(intptr_t)20));
    
    TEST_ASSERT_EQUAL_INT(0, g_test_counter); // Not run yet
    
    async_poll(&queue);
    TEST_ASSERT_EQUAL_INT(30, g_test_counter); // 10 + 20
    
    // 3. Test Queue Full
    // Size is 4, so capacity is 3 (one slot reserved)
    TEST_ASSERT_TRUE(async_submit(&queue, test_task_add, (void*)1));
    TEST_ASSERT_TRUE(async_submit(&queue, test_task_add, (void*)1));
    TEST_ASSERT_TRUE(async_submit(&queue, test_task_add, (void*)1));
    TEST_ASSERT_FALSE(async_submit(&queue, test_task_add, (void*)1)); // Should fail
    
    g_test_counter = 0;
    async_poll(&queue);
    TEST_ASSERT_EQUAL_INT(3, g_test_counter);
}

