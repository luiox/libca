/* Auto-migrated from src/em_util/singly_list.c test blocks */
#include "singly_list.h"


#include <em_test/test.h>

typedef struct test_item
{
    i32          value;
    slist_node_t node;
} test_item_t;

static test_item_t* test_item_from_node(slist_node_t* node)
{
    return (test_item_t*)((u8*)node - offsetof(test_item_t, node));
}

TEST_CASE(singly_list_init_and_empty)
{
    slist_node_t head;
    slist_init(&head);

    TEST_ASSERT(slist_is_empty(&head));
    TEST_ASSERT_NULL(slist_front(&head));
    TEST_ASSERT_EQUAL_UINT(0, (u32)slist_len(&head));
}

TEST_CASE(singly_list_push_pop_front)
{
    slist_node_t head;
    slist_init(&head);

    test_item_t item1 = {.value = 1};
    test_item_t item2 = {.value = 2};
    test_item_t item3 = {.value = 3};

    slist_push_front(&head, &item1.node);
    slist_push_front(&head, &item2.node);
    slist_push_front(&head, &item3.node);

    TEST_ASSERT_EQUAL_UINT(3, (u32)slist_len(&head));
    TEST_ASSERT_EQUAL_PTR(&item3.node, slist_front(&head));

    slist_node_t* n = slist_pop_front(&head);
    TEST_ASSERT_EQUAL_PTR(&item3.node, n);
    TEST_ASSERT_NULL(n->next);

    n = slist_pop_front(&head);
    TEST_ASSERT_EQUAL_PTR(&item2.node, n);
    TEST_ASSERT_NULL(n->next);

    n = slist_pop_front(&head);
    TEST_ASSERT_EQUAL_PTR(&item1.node, n);
    TEST_ASSERT_NULL(n->next);

    TEST_ASSERT_NULL(slist_pop_front(&head));
    TEST_ASSERT(slist_is_empty(&head));
}

TEST_CASE(singly_list_insert_and_remove_after)
{
    slist_node_t head;
    slist_init(&head);

    test_item_t item1 = {.value = 1};
    test_item_t item2 = {.value = 2};
    test_item_t item3 = {.value = 3};

    slist_push_front(&head, &item1.node);
    slist_insert_after(&item1.node, &item2.node);
    slist_insert_after(&item2.node, &item3.node);

    TEST_ASSERT_EQUAL_UINT(3, (u32)slist_len(&head));

    slist_node_t* removed = slist_remove_after(&item1.node);
    TEST_ASSERT_EQUAL_PTR(&item2.node, removed);
    TEST_ASSERT_NULL(removed->next);
    TEST_ASSERT_EQUAL_PTR(&item3.node, item1.node.next);
    TEST_ASSERT_EQUAL_UINT(2, (u32)slist_len(&head));

    removed = slist_remove_after(&item3.node);
    TEST_ASSERT_NULL(removed);
}

TEST_CASE(singly_list_for_each)
{
    slist_node_t head;
    slist_init(&head);

    test_item_t item1 = {.value = 10};
    test_item_t item2 = {.value = 20};
    test_item_t item3 = {.value = 30};

    slist_push_front(&head, &item3.node);
    slist_push_front(&head, &item2.node);
    slist_push_front(&head, &item1.node);

    i32 sum   = 0;
    u32 count = 0;
    slist_for_each(node, &head)
    {
        test_item_t* item = test_item_from_node(node);
        sum += item->value;
        count++;
    }

    TEST_ASSERT_EQUAL_INT(60, sum);
    TEST_ASSERT_EQUAL_UINT(3, count);
}

TEST_CASE(singly_list_for_each_safe_remove_even)
{
    slist_node_t head;
    slist_init(&head);

    test_item_t items[6];
    for (i32 i = 0; i < 6; i++) {
        items[i].value = i;
        slist_push_front(&head, &items[5 - i].node);
    }

    slist_node_t* prev = &head;
    slist_for_each_safe(node, &head, next_node)
    {
        test_item_t* item = test_item_from_node(node);
        if ((item->value % 2) == 0) {
            slist_node_t* removed = slist_remove_after(prev);
            TEST_ASSERT_EQUAL_PTR(node, removed);
            continue;
        }
        prev = node;
    }

    TEST_ASSERT_EQUAL_UINT(3, (u32)slist_len(&head));

    i32 expected[] = {1, 3, 5};
    i32 index      = 0;
    slist_for_each(node, &head)
    {
        test_item_t* item = test_item_from_node(node);
        TEST_ASSERT_EQUAL_INT(expected[index], item->value);
        index++;
    }
    TEST_ASSERT_EQUAL_INT(3, index);
}
