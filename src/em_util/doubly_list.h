/**
 * @file doubly_list.h
 * @author canrad (1517807724@qq.com)
 * @brief 侵入式双向循环列表
 * @version 0.1
 * @date 2026-03-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_UTIL_DOUBLY_LIST_H
#define LIBCA_EM_UTIL_DOUBLY_LIST_H

#include "../em_base/datatype.h"
#include "../em_base/macro_util.h"

typedef struct dlist_node{
    struct dlist_node *prev;
    struct dlist_node *next;
}dlist_node_t;

// 初始化链表头（循环链表）
static inline void dlist_init(dlist_node_t *head) {
    head->prev = head;
    head->next = head;
}

// 初始化普通节点（未入链）
static inline void dlist_node_init(dlist_node_t *node) {
    node->prev = node;
    node->next = node;
}

// 判断链表是否为空
static inline bool dlist_is_empty(const dlist_node_t *head) {
    return head->next == head;
}

// 判断节点是否已链接到某个链表
static inline bool dlist_is_linked(const dlist_node_t *node) {
    return node->next != node;
}

// 内部插入：把 node 插入到 prev 与 next 之间
static inline void dlist_insert_between(dlist_node_t *prev, dlist_node_t *next, dlist_node_t *node) {
    node->prev = prev;
    node->next = next;
    prev->next = node;
    next->prev = node;
}

// 头插
static inline void dlist_push_front(dlist_node_t *head, dlist_node_t *node) {
    dlist_insert_between(head, head->next, node);
}

// 尾插
static inline void dlist_push_back(dlist_node_t *head, dlist_node_t *node) {
    dlist_insert_between(head->prev, head, node);
}

// 删除节点（不会释放内存）
static inline void dlist_remove(dlist_node_t *node) {
    dlist_node_t *prev = node->prev;
    dlist_node_t *next = node->next;
    prev->next = next;
    next->prev = prev;
    dlist_node_init(node);
}

// 获取首节点（空链表返回 NULL）
static inline dlist_node_t *dlist_front(const dlist_node_t *head) {
    return dlist_is_empty(head) ? NULL : head->next;
}

// 获取尾节点（空链表返回 NULL）
static inline dlist_node_t *dlist_back(const dlist_node_t *head) {
    return dlist_is_empty(head) ? NULL : head->prev;
}

// 弹出首节点
static inline dlist_node_t *dlist_pop_front(dlist_node_t *head) {
    if (dlist_is_empty(head)) {
        return NULL;
    }
    dlist_node_t *node = head->next;
    dlist_remove(node);
    return node;
}

// 弹出尾节点
static inline dlist_node_t *dlist_pop_back(dlist_node_t *head) {
    if (dlist_is_empty(head)) {
        return NULL;
    }
    dlist_node_t *node = head->prev;
    dlist_remove(node);
    return node;
}

// 链表长度
static inline size_t dlist_len(const dlist_node_t *head) {
    size_t count = 0;
    const dlist_node_t *node = head->next;
    while (node != head) {
        count++;
        node = node->next;
    }
    return count;
}

// 遍历
#define dlist_for_each(node, head) \
    for (dlist_node_t *node = (head)->next; node != (head); node = node->next)

// 安全遍历（允许删除当前节点）
#define dlist_for_each_safe(node, head, next_node) \
    for (dlist_node_t *node = (head)->next, *next_node = NULL; \
         node != (head) && (next_node = node->next, 1); \
         node = next_node)

// 反向遍历
#define dlist_for_each_reverse(node, head) \
    for (dlist_node_t *node = (head)->prev; node != (head); node = node->prev)

// 由节点指针获取宿主结构
#define dlist_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif // !LIBCA_EM_UTIL_DOUBLY_LIST_H
