#include "doubly_linked_list.h"
#include <stdlib.h>
#include <assert.h>

// 初始化双向链表
void doubly_linked_list_init(doubly_linked_list_t* list)
{
    assert(list);
    list->size = 0;
    list->head = NULL;
    list->tail = NULL;
}

// 初始化双向链表结点
void doubly_linked_list_node_init(doubly_linked_list_node_t* node, void* data)
{
    assert(node);
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
}

// 在双向链表的前面加上一个结点
void doubly_linked_list_push_front(doubly_linked_list_t* list, doubly_linked_list_node_t* node)
{
    assert(list);
    assert(node);
    if (list->size == 0) {
        // 头结点没有的情况，直接设置头尾指针
        list->head = node;
        list->tail = node;
    }
    else {
        // 先把原来头结点的上一个结点设置为要插入结点
        list->head->prev = node;
        // 把要插入结点的下一个结点设置为原来头结点
        node->next = list->head;
        // 把要插入结点设置为头结点
        list->head = node;
    }
    list->size++;
}

// 在双向链表的后面加上一个结点
void doubly_linked_list_push_back(doubly_linked_list_t* list, doubly_linked_list_node_t* node)
{
    assert(list);
    assert(node);
    if (list->size == 0) {
        // 头结点没有的情况，直接设置头尾指针
        list->head = node;
        list->tail = node;
    }
    else {
        // 先把原来尾结点的下一个结点设置为要插入结点
        list->tail->next = node;
        // 把要插入结点的上一个结点设置为原来尾结点
        node->prev = list->tail;
        // 把要插入结点设置为尾结点
        list->tail = node;
    }
    list->size++;
}

// 移除双向链表中最后面的结点，并返回这个结点的地址
doubly_linked_list_node_t* doubly_linked_list_pop_back(doubly_linked_list_t* list)
{
    assert(list);
    if (list->size == 0) {
        return NULL;
    }

    doubly_linked_list_node_t* node = list->tail;
    if (list->size == 1) {
        // 如果只有一个结点，直接把头尾指针都置空
        list->head = NULL;
        list->tail = NULL;
    }
    else {
        // 把原来尾结点的上一个结点的next结点设置为空
        list->tail->prev->next = NULL;
        // 把原来尾结点的上一个结点设置为尾结点
        list->tail = list->tail->prev;
    }
    list->size--;
    return node;
}

// 移除双向链表中最前面的结点，并返回这个结点的地址
doubly_linked_list_node_t* doubly_linked_list_pop_front(doubly_linked_list_t* list)
{
    assert(list);
    if (list->size == 0) {
        return NULL;
    }

    doubly_linked_list_node_t* node = list->head;
    if (list->size == 1) {
        // 如果只有一个结点，直接把头尾指针都置空
        list->head = NULL;
        list->tail = NULL;
    }
    else {
        // 把原来头结点的下一个结点的prev设置为空
        list->head->next->prev = NULL;
        // 把原来头结点的下一个结点设置为头结点
        list->head = list->tail->prev;
    }
    list->size--;
    return node;
}

///////////////////////////////////////////////////////////////////////////////

// 创建一个双向链表
doubly_linked_list_t* doubly_linked_list_create()
{
    doubly_linked_list_t* list = (doubly_linked_list_t*)malloc(sizeof(doubly_linked_list_t));
    doubly_linked_list_init(list);
    return list;
}

// 创建一个双向链表节点
doubly_linked_list_node_t* doubly_linked_list_node_create(void* data)
{
    doubly_linked_list_node_t* node =
        (doubly_linked_list_node_t*)malloc(sizeof(doubly_linked_list_node_t));
    doubly_linked_list_node_init(node, data);
    return node;
}

///////////////////////////////////////////////////////////////////////////////

// 获取开始的节点
doubly_linked_list_node_t* doubly_linked_list_begin(doubly_linked_list_t* list)
{
    assert(list);
    return list->head;
}

// 获取结束的节点
doubly_linked_list_node_t* doubly_linked_list_end(doubly_linked_list_t* list)
{
    assert(list);
    return list->tail;
}

// 获取下一个节点
doubly_linked_list_node_t* doubly_linked_list_next(doubly_linked_list_node_t* node)
{
    assert(node);
    return node->next;
}

// 获取上一个节点
doubly_linked_list_node_t* doubly_linked_list_prev(doubly_linked_list_node_t* node)
{
    assert(node);
    return node->prev;
}
