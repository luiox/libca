#include <libca/container/doubly_linked_list.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <libca/core/test.h>

TEST_CASE(doubly_linked_list_test1)
{
    int                        d[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    doubly_linked_list_node_t* node[10];
    doubly_linked_list_t*      list = doubly_linked_list_create();
    // 插入int数据
    for (int i = 0; i < 10; i++) {
        node[i] = doubly_linked_list_node_create(&d[i]);
        doubly_linked_list_push_back(list, node[i]);
    }

    DOUBLE_LINKED_LIST_FOR_EACH(it, list)
    {
        // do something with it
        printf("%d\n", *(int*)it->data);
    }
    DOUBLE_LINKED_LIST_REVERSE_FOR_EACH(it, list)
    {
        // do something with it
        printf("%d\n", *(int*)it->data);
    }
}

TEST_CASE(doubly_linked_list_test2)
{
    // Test case 1: Initialize the doubly linked list
    doubly_linked_list_t list1;
    doubly_linked_list_init(&list1);
    assert(list1.head == NULL);
    assert(list1.tail == NULL);
    assert(list1.size == 0);

    // Test case 2: Create a new node in the list
    doubly_linked_list_node_t* node1 = doubly_linked_list_node_create(NULL);
    doubly_linked_list_node_init(node1, "data1");
    assert(strncmp(node1->data, "data1", strlen("data1")) == 0);
    assert(node1->next == NULL);
    assert(node1->prev == NULL);

    // Test case 3: Push a new node to the front of the list
    doubly_linked_list_t list2;
    doubly_linked_list_init(&list2);
    doubly_linked_list_push_front(&list2, node1);
    assert(list2.head == node1);
    assert(list2.tail == node1);
    assert(list2.size == 1);

    // Test case 4: Push a new node to the back of the list
    doubly_linked_list_node_t* node2 = doubly_linked_list_node_create(NULL);
    doubly_linked_list_node_init(node2, "data2");
    doubly_linked_list_push_back(&list2, node2);
    assert(list2.head == node1);
    assert(list2.tail == node2);
    assert(list2.size == 2);

    // Test case 5: Pop a node from the back of the list
    doubly_linked_list_node_t* removed_node = doubly_linked_list_pop_back(&list2);
    assert(removed_node == node2);
    assert(list2.head == node1);
    assert(list2.tail == node1);
    assert(list2.size == 1);

    // Test case 6: Pop a node from the front of the list
    doubly_linked_list_node_t* removed_node2 = doubly_linked_list_pop_front(&list2);
    assert(removed_node2 == node1);
    assert(list2.head == NULL);
    assert(list2.tail == NULL);
    assert(list2.size == 0);

    // Test case 7: Iterate through the list
    doubly_linked_list_t list3;
    doubly_linked_list_init(&list3);
    doubly_linked_list_node_t* node3 = doubly_linked_list_node_create(NULL);
    doubly_linked_list_node_init(node3, "data3");
    doubly_linked_list_push_back(&list3, node3);
    DOUBLE_LINKED_LIST_FOR_EACH(current_node, &list3)
    {
        if (current_node == node3) {
            break;
        }
        assert(current_node == NULL);
    }

    // Test case 8: Reverse iterate through the list
    doubly_linked_list_t list4;
    doubly_linked_list_init(&list4);
    doubly_linked_list_node_t* node4 = doubly_linked_list_node_create(NULL);
    doubly_linked_list_node_init(node4, "data4");
    doubly_linked_list_push_back(&list4, node4);
    DOUBLE_LINKED_LIST_REVERSE_FOR_EACH(current_node, &list4)
    {
        if (current_node == node4) {
            break;
        }
        assert(current_node == NULL);
    }

    printf("All test cases pass");
}

// int main(int argc, char* argv[])
// {
//     // test1();
//     test2();

//     return 0;
// }
