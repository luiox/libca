/**
 * @file singly_list.h
 * @author canrad (1517807724@qq.com)
 * @brief 侵入式单向链表
 * @version 0.1
 * @date 2026-03-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_UTIL_SINGLY_LIST_H
#define LIBCA_EM_UTIL_SINGLY_LIST_H

#include "../em_base/datatype.h"
#include "../em_base/macro_util.h"

typedef struct slist_node {
    struct slist_node *next;
} slist_node_t;

// 初始化链表头
static inline void slist_init(slist_node_t *head) {
    head->next = NULL;
}

// 在链表头部插入节点
static inline void slist_push_front(slist_node_t *head, slist_node_t *node) {
    node->next = head->next;
    head->next = node;
}

// 从链表头部移除节点并返回，若链表为空返回 NULL
static inline slist_node_t *slist_pop_front(slist_node_t *head) {
    slist_node_t *first = head->next;
    if (first) {
        head->next = first->next;
        // 断开原节点的 next，防止意外使用
        first->next = NULL;
    }
    return first;
}

// 判断链表是否为空
static inline bool slist_is_empty(const slist_node_t *head) {
    return head->next == NULL;
}

// 获取第一个节点但不弹出
static inline slist_node_t *slist_front(const slist_node_t *head) {
    return head->next;
}

// 遍历链表（使用 for 循环）
#define slist_for_each(node, head) \
    for (slist_node_t *node = (head)->next; node; node = node->next)

// 安全遍历，允许在遍历时删除当前节点（使用临时变量 next）
#define slist_for_each_safe(node, head, next_node) \
    for (slist_node_t *node = (head)->next, *next_node; \
         node && (next_node = node->next, 1); \
         node = next_node)

// 专门为slist链表封装的便捷宏
#define slist_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// 在指定节点后插入 (通用操作)
static inline void slist_insert_after(slist_node_t *node, slist_node_t *new_node) {
    new_node->next = node->next;
    node->next = new_node;
}

// 删除指定节点后的节点 (单向链表无法 O(1) 删除当前节点，只能删后继)
// 返回被删除的节点，若无可删则返回 NULL
static inline slist_node_t *slist_remove_after(slist_node_t *node) {
    slist_node_t *removed = node->next;
    if (removed) {
        node->next = removed->next;
        removed->next = NULL; // 安全断开
    }
    return removed;
}

static inline size_t slist_len(const slist_node_t *head) {
    size_t count = 0;
    slist_for_each(node, head) {
        count++;
    }
    return count;
}


#endif // !LIBCA_EM_UTIL_SINGLY_LIST_H
