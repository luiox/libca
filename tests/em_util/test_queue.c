/* Auto-migrated from src/em_util/queue.c test blocks */
#include "queue.h"
#include <stdlib.h>


#include <em_test/test.h>

// 用于测试的数据类型
typedef struct
{
    int value;
} test_data_t;

// 用于比较两个test_data_t数据是否相同的辅助函数
static bool test_data_equal(void* a, void* b)
{
    test_data_t* data_a = (test_data_t*)a;
    test_data_t* data_b = (test_data_t*)b;
    return data_a->value == data_b->value;
}

// queue基础操作测试
TEST_CASE(queue_basic)
{
    queue_t     queue;
    test_data_t data1 = {1};
    test_data_t data2 = {2};
    test_data_t data3 = {3};
    test_data_t pop_data;

    // 测试1: 初始化队列
    queue_init(&queue);
    TEST_ASSERT(queue_empty(&queue));   // 队列应该为空

    // 测试2: 向队列中添加元素
    queue_push(&queue, &data1);
    TEST_ASSERT(test_data_equal(queue_front(&queue), &data1));   // 队头应该是data1
    TEST_ASSERT_EQUAL_INT(1, (int)queue_size(&queue));           // 队列大小应该是1

    // 测试3: 再次向队列中添加元素
    queue_push(&queue, &data2);
    TEST_ASSERT_EQUAL_INT(2, (int)queue_size(&queue));   // 队列大小应该是2

    // 测试4: 从队列中弹出元素
    pop_data = *(test_data_t*)queue_front(&queue);
    queue_pop(&queue);
    TEST_ASSERT(test_data_equal(&pop_data, &data1));     // 弹出的应该是data1
    TEST_ASSERT_EQUAL_INT(1, (int)queue_size(&queue));   // 队列大小应该是1

    // 测试5: 清空队列
    queue_clear(&queue);
    TEST_ASSERT(queue_empty(&queue));   // 队列应该为空

    // 测试6: 向空队列中添加和弹出元素
    queue_push(&queue, &data3);
    // 队列不应该为空
    TEST_ASSERT(!queue_empty(&queue));
    queue_pop(&queue);
    TEST_ASSERT(queue_empty(&queue));   // 队列应该为空
}

