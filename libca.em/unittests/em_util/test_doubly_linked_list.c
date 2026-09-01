/* Auto-migrated from src/em_util/doubly_linked_list.c test blocks */
#include "doubly_linked_list.h"
#include <stdlib.h>
#include <em_base/debug.h>


#include <em_test/test.h>

TEST_CASE(doubly_linked_list_init)
{
    doubly_linked_list_t list;
    doubly_linked_list_init(&list);
    TEST_ASSERT_NULL(list.head);
    TEST_ASSERT_NULL(list.tail);
    TEST_ASSERT_EQUAL_UINT(0, list.size);
}

TEST_CASE(doubly_linked_list_push_pop)
{
    doubly_linked_list_t list;
    doubly_linked_list_init(&list);

    doubly_linked_list_node_t node1, node2;
    doubly_linked_list_node_init(&node1, (void*)1);
    doubly_linked_list_node_init(&node2, (void*)2);

    // Test push_back
    doubly_linked_list_push_back(&list, &node1);
    TEST_ASSERT_EQUAL_PTR(&node1, list.head);
    TEST_ASSERT_EQUAL_PTR(&node1, list.tail);
    TEST_ASSERT_EQUAL_UINT(1, list.size);

    doubly_linked_list_push_back(&list, &node2);
    TEST_ASSERT_EQUAL_PTR(&node1, list.head);
    TEST_ASSERT_EQUAL_PTR(&node2, list.tail);
    TEST_ASSERT_EQUAL_UINT(2, list.size);

    // Test pop_back
    doubly_linked_list_node_t* popped = doubly_linked_list_pop_back(&list);
    TEST_ASSERT_EQUAL_PTR(&node2, popped);
    TEST_ASSERT_EQUAL_PTR(&node1, list.head);
    TEST_ASSERT_EQUAL_PTR(&node1, list.tail);
    TEST_ASSERT_EQUAL_UINT(1, list.size);

    // Test push_front
    doubly_linked_list_push_front(&list, &node2);
    TEST_ASSERT_EQUAL_PTR(&node2, list.head);
    TEST_ASSERT_EQUAL_PTR(&node1, list.tail);
    TEST_ASSERT_EQUAL_UINT(2, list.size);

    // Test pop_front
    popped = doubly_linked_list_pop_front(&list);
    TEST_ASSERT_EQUAL_PTR(&node2, popped);
    TEST_ASSERT_EQUAL_PTR(&node1, list.head);
    TEST_ASSERT_EQUAL_PTR(&node1, list.tail);
    TEST_ASSERT_EQUAL_UINT(1, list.size);
}

TEST_CASE(doubly_linked_list_iteration)
{
    doubly_linked_list_t list;
    doubly_linked_list_init(&list);

    int                       data[3] = {1, 2, 3};
    doubly_linked_list_node_t nodes[3];
    for (int i = 0; i < 3; i++) {
        doubly_linked_list_node_init(&nodes[i], &data[i]);
        doubly_linked_list_push_back(&list, &nodes[i]);
    }

    int count = 0;
    DOUBLE_LINKED_LIST_FOR_EACH(it, &list)
    {
        TEST_ASSERT_EQUAL_INT(data[count], *(int*)it->data);
        count++;
    }
    TEST_ASSERT_EQUAL_INT(3, count);

    count = 2;
    DOUBLE_LINKED_LIST_REVERSE_FOR_EACH(it, &list)
    {
        TEST_ASSERT_EQUAL_INT(data[count], *(int*)it->data);
        count--;
    }
    TEST_ASSERT_EQUAL_INT(-1, count);
}

TEST_CASE(doubly_linked_list_dynamic)
{
    doubly_linked_list_t* list = doubly_linked_list_create();
    TEST_ASSERT(list != NULL);
    TEST_ASSERT_EQUAL_INT(0, (int)list->size);

    int                        val  = 100;
    doubly_linked_list_node_t* node = doubly_linked_list_node_create(&val);
    TEST_ASSERT(node != NULL);
    TEST_ASSERT(node->data == &val);

    doubly_linked_list_push_back(list, node);
    TEST_ASSERT_EQUAL_INT(1, (int)list->size);
    TEST_ASSERT(list->head->data == &val);

    doubly_linked_list_node_t* popped = doubly_linked_list_pop_front(list);
    TEST_ASSERT(popped == node);
    TEST_ASSERT_EQUAL_INT(0, (int)list->size);

    free(node);
    free(list);
}
