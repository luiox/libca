#include "queue.h"
#include <stdlib.h>

// 初始化队列
void queue_init(queue_t* queue)
{
    doubly_linked_list_init(queue);
}

// 加入一个元素到对位
void queue_push(queue_t* queue, void* data)
{
    doubly_linked_list_node_t* node = doubly_linked_list_node_create(data);
    if (NULL == node)
        return;
    doubly_linked_list_push_back(queue, node);
}

// 弹出队头元素
void queue_pop(queue_t* queue)
{
    doubly_linked_list_node_t* head = doubly_linked_list_pop_front(queue);
    if (NULL == head)
        return;
    free(head);
}

// 获取队头元素
void* queue_front(queue_t* queue)
{
    doubly_linked_list_node_t* head = queue->head;
    if (NULL == head) {
        return NULL;
    }
    return head->data;
}

// 获取队列大小
int32_t queue_size(queue_t* queue)
{
    return queue->size;
}

// 判断队列是否为空
bool queue_empty(queue_t* queue)
{
    return queue->size == 0;
}

// 清空队列
void queue_clear(queue_t* queue)
{
    doubly_linked_list_node_t* node = queue->head;
    while (NULL != node) {
        doubly_linked_list_node_t* next = node->next;
        free(node);
        node = next;
    }
    queue->size = 0;
}

#if TEST_ENABLE

#    include "../em_test/test.h"

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

#endif
