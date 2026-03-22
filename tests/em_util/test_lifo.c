/* Auto-migrated from src/em_util/lifo.c test blocks */
#include "lifo.h"


#include <em_test/test.h>

typedef struct lifo_test_item {
    i32 value;
    lifo_node_t node;
} lifo_test_item_t;

TEST_CASE(lifo_init_and_empty)
{
    lifo_t lifo;
    lifo_init(&lifo);

    TEST_ASSERT(lifo_is_empty(&lifo));
    TEST_ASSERT_EQUAL_UINT(0U, (u32)lifo_size(&lifo));
    TEST_ASSERT_NULL(lifo_peek(&lifo));
    TEST_ASSERT_NULL(lifo_pop(&lifo));
}

TEST_CASE(lifo_push_pop_order)
{
    lifo_t lifo;
    lifo_init(&lifo);

    lifo_test_item_t item1 = {.value = 1};
    lifo_test_item_t item2 = {.value = 2};
    lifo_test_item_t item3 = {.value = 3};

    lifo_push(&lifo, &item1.node);
    lifo_push(&lifo, &item2.node);
    lifo_push(&lifo, &item3.node);

    TEST_ASSERT_EQUAL_UINT(3U, (u32)lifo_size(&lifo));
    TEST_ASSERT_EQUAL_PTR(&item3.node, lifo_peek(&lifo));

    lifo_node_t *node = lifo_pop(&lifo);
    TEST_ASSERT_EQUAL_PTR(&item3.node, node);
    TEST_ASSERT_NULL(node->next);

    node = lifo_pop(&lifo);
    TEST_ASSERT_EQUAL_PTR(&item2.node, node);
    TEST_ASSERT_NULL(node->next);

    node = lifo_pop(&lifo);
    TEST_ASSERT_EQUAL_PTR(&item1.node, node);
    TEST_ASSERT_NULL(node->next);

    TEST_ASSERT(lifo_is_empty(&lifo));
    TEST_ASSERT_EQUAL_UINT(0U, (u32)lifo_size(&lifo));
}

TEST_CASE(lifo_entry_test)
{
    lifo_t lifo;
    lifo_init(&lifo);

    lifo_test_item_t item1 = {.value = 11};
    lifo_test_item_t item2 = {.value = 22};

    lifo_push(&lifo, &item1.node);
    lifo_push(&lifo, &item2.node);

    lifo_node_t *node = lifo_pop(&lifo);
    lifo_test_item_t *item = lifo_entry(node, lifo_test_item_t, node);
    TEST_ASSERT_EQUAL_INT(22, item->value);

    node = lifo_pop(&lifo);
    item = lifo_entry(node, lifo_test_item_t, node);
    TEST_ASSERT_EQUAL_INT(11, item->value);
}

