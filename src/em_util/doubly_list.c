#include "doubly_list.h"

#if TEST_ENABLE

#include "../em_test/test.h"

typedef struct test_dlist_item {
    i32 value;
    dlist_node_t node;
} test_dlist_item_t;

TEST_CASE(dlist_init_and_empty)
{
    dlist_node_t head;
    dlist_init(&head);

    TEST_ASSERT(dlist_is_empty(&head));
    TEST_ASSERT_EQUAL_PTR(&head, head.next);
    TEST_ASSERT_EQUAL_PTR(&head, head.prev);
    TEST_ASSERT_NULL(dlist_front(&head));
    TEST_ASSERT_NULL(dlist_back(&head));
    TEST_ASSERT_EQUAL_UINT(0, (u32)dlist_len(&head));
}

TEST_CASE(dlist_push_front_and_back)
{
    dlist_node_t head;
    dlist_init(&head);

    test_dlist_item_t item1 = {.value = 1};
    test_dlist_item_t item2 = {.value = 2};
    test_dlist_item_t item3 = {.value = 3};
    dlist_node_init(&item1.node);
    dlist_node_init(&item2.node);
    dlist_node_init(&item3.node);

    dlist_push_front(&head, &item2.node);  // [2]
    dlist_push_front(&head, &item1.node);  // [1,2]
    dlist_push_back(&head, &item3.node);   // [1,2,3]

    TEST_ASSERT_EQUAL_UINT(3, (u32)dlist_len(&head));
    TEST_ASSERT_EQUAL_PTR(&item1.node, dlist_front(&head));
    TEST_ASSERT_EQUAL_PTR(&item3.node, dlist_back(&head));
    TEST_ASSERT(dlist_is_linked(&item1.node));
    TEST_ASSERT(dlist_is_linked(&item2.node));
    TEST_ASSERT(dlist_is_linked(&item3.node));

    i32 expected[] = {1, 2, 3};
    i32 index = 0;
    dlist_for_each(node, &head) {
        test_dlist_item_t *item = dlist_entry(node, test_dlist_item_t, node);
        TEST_ASSERT_EQUAL_INT(expected[index], item->value);
        index++;
    }
    TEST_ASSERT_EQUAL_INT(3, index);
}

TEST_CASE(dlist_remove_and_pop)
{
    dlist_node_t head;
    dlist_init(&head);

    test_dlist_item_t item1 = {.value = 1};
    test_dlist_item_t item2 = {.value = 2};
    test_dlist_item_t item3 = {.value = 3};
    dlist_node_init(&item1.node);
    dlist_node_init(&item2.node);
    dlist_node_init(&item3.node);

    dlist_push_back(&head, &item1.node);
    dlist_push_back(&head, &item2.node);
    dlist_push_back(&head, &item3.node);

    dlist_remove(&item2.node);
    TEST_ASSERT_EQUAL_UINT(2, (u32)dlist_len(&head));
    TEST_ASSERT(!dlist_is_linked(&item2.node));

    dlist_node_t *node = dlist_pop_front(&head);
    TEST_ASSERT_EQUAL_PTR(&item1.node, node);
    TEST_ASSERT(!dlist_is_linked(node));

    node = dlist_pop_back(&head);
    TEST_ASSERT_EQUAL_PTR(&item3.node, node);
    TEST_ASSERT(!dlist_is_linked(node));

    TEST_ASSERT(dlist_is_empty(&head));
    TEST_ASSERT_NULL(dlist_pop_front(&head));
    TEST_ASSERT_NULL(dlist_pop_back(&head));
}

TEST_CASE(dlist_reverse_iteration)
{
    dlist_node_t head;
    dlist_init(&head);

    test_dlist_item_t items[4];
    for (i32 i = 0; i < 4; i++) {
        items[i].value = i + 1;
        dlist_node_init(&items[i].node);
        dlist_push_back(&head, &items[i].node);
    }

    i32 expected = 4;
    dlist_for_each_reverse(node, &head) {
        test_dlist_item_t *item = dlist_entry(node, test_dlist_item_t, node);
        TEST_ASSERT_EQUAL_INT(expected, item->value);
        expected--;
    }
    TEST_ASSERT_EQUAL_INT(0, expected);
}

TEST_CASE(dlist_for_each_safe_remove_even)
{
    dlist_node_t head;
    dlist_init(&head);

    test_dlist_item_t items[6];
    for (i32 i = 0; i < 6; i++) {
        items[i].value = i;
        dlist_node_init(&items[i].node);
        dlist_push_back(&head, &items[i].node);
    }

    dlist_for_each_safe(node, &head, next_node) {
        test_dlist_item_t *item = dlist_entry(node, test_dlist_item_t, node);
        if ((item->value % 2) == 0) {
            dlist_remove(node);
        }
    }

    TEST_ASSERT_EQUAL_UINT(3, (u32)dlist_len(&head));

    i32 expected[] = {1, 3, 5};
    i32 index = 0;
    dlist_for_each(node, &head) {
        test_dlist_item_t *item = dlist_entry(node, test_dlist_item_t, node);
        TEST_ASSERT_EQUAL_INT(expected[index], item->value);
        index++;
    }
    TEST_ASSERT_EQUAL_INT(3, index);
}

#endif
