#include <libca/container/queue.h>
#include <stdio.h>
#include <assert.h>
#include <libca/core/test.h>

// 用于测试的数据类型
typedef struct
{
    int value;
} test_data_t;

// 用于比较两个test_data_t数据是否相同的辅助函数
bool test_data_equal(void* a, void* b)
{
    test_data_t* data_a = (test_data_t*)a;
    test_data_t* data_b = (test_data_t*)b;
    return data_a->value == data_b->value;
}

TEST_CASE(queue_test1)
{
    queue_t     queue;
    test_data_t data1 = {1};
    test_data_t data2 = {2};
    test_data_t data3 = {3};
    test_data_t pop_data;

    // 测试1: 初始化队列
    queue_init(&queue);
    assert(queue_empty(&queue));   // 队列应该为空

    // 测试2: 向队列中添加元素
    queue_push(&queue, &data1);
    assert(test_data_equal(queue_front(&queue), &data1));   // 队头应该是data1
    assert(queue_size(&queue) == 1);                        // 队列大小应该是1

    // 测试3: 再次向队列中添加元素
    queue_push(&queue, &data2);
    assert(queue_size(&queue) == 2);   // 队列大小应该是2

    // 测试4: 从队列中弹出元素
    pop_data = *(test_data_t*)queue_front(&queue);
    queue_pop(&queue);
    assert(test_data_equal(&pop_data, &data1));   // 弹出的应该是data1
    assert(queue_size(&queue) == 1);              // 队列大小应该是1

    // 测试5: 清空队列
    queue_clear(&queue);
    assert(queue_empty(&queue));   // 队列应该为空

    // 测试6: 向空队列中添加和弹出元素
    queue_push(&queue, &data3);
    assert(!queue_empty(&queue));   // 队列不应该为空
    queue_pop(&queue);
    assert(queue_empty(&queue));   // 队列应该为空

    printf("All tests passed.\n");
}

// int main(int argc, char* argv[])
// {
//     test1();

//     return 0;
// }
