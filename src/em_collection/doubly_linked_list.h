/*********************************************************************
 * @file   doubly_linked_list.h
 * @brief  一个简单的双向链表实现
 *
 * @author Canrad
 * @date   2024.06.02
 *********************************************************************/

#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

#include "../em_base/datatype.h"

// 双向链表的结点
typedef struct doubly_linked_list_node
{
    void*                           data;   // 数据
    struct doubly_linked_list_node* next;   // 下一个结点
    struct doubly_linked_list_node* prev;   // 上一个结点
} doubly_linked_list_node_t;

// 双向链表
typedef struct doubly_linked_list
{
    doubly_linked_list_node_t* head;   // 头结点
    doubly_linked_list_node_t* tail;   // 尾结点
    usize                      size;   // 链表大小
} doubly_linked_list_t;

///////////////////////////////////////////////////////////////////////////////
// 以下为双向链表的操作，内部没有任何的动态内存分配操作

/**
 * @brief 初始化双向链表
 * @param list 双向链表
 */
void doubly_linked_list_init(doubly_linked_list_t* list);

/**
 * @brief 初始化双向链表结点
 * @param node 双向链表结点
 * @param data 数据
 */
void doubly_linked_list_node_init(doubly_linked_list_node_t* node, void* data);

/**
 * @brief 在双向链表的前面加上一个结点
 * @param list 双向链表
 * @param node 双向链表结点
 */
void doubly_linked_list_push_front(doubly_linked_list_t* list, doubly_linked_list_node_t* node);

/**
 * @brief 在双向链表的后面加上一个结点
 * @param list 双向链表
 * @param node 双向链表结点
 */
void doubly_linked_list_push_back(doubly_linked_list_t* list, doubly_linked_list_node_t* node);

/**
 * @brief 移除双向链表中最后面的结点
 * @param list 双向链表
 * @return 返回被移除结点的地址
 */
doubly_linked_list_node_t* doubly_linked_list_pop_back(doubly_linked_list_t* list);

/**
 * @brief 移除双向链表中最前面的结点
 * @param list 双向链表
 * @return 返回被移除结点的地址
 */
doubly_linked_list_node_t* doubly_linked_list_pop_front(doubly_linked_list_t* list);

///////////////////////////////////////////////////////////////////////////////
// 以下是通过动态内存申请来产生结点的帮助函数，如果不需要则可以使用静态的创建方法

/**
 * @brief 创建一个双向链表
 * @return 双向链表
 */
doubly_linked_list_t* doubly_linked_list_create();

/**
 * @brief 创建一个双向链表结点
 * @return 双向链表结点
 */
doubly_linked_list_node_t* doubly_linked_list_node_create(void* data);

///////////////////////////////////////////////////////////////////////////////
// 以下为了服务遍历双向链表的宏

/**
 * @brief 获取开始的结点
 * @param list 双向链表
 * @return 双向链表开始的结点
 */
doubly_linked_list_node_t* doubly_linked_list_begin(doubly_linked_list_t* list);

/**
 * @brief 获取结束的结点
 * @param list 双向链表
 * @return 双向链表结束的结点
 */
doubly_linked_list_node_t* doubly_linked_list_end(doubly_linked_list_t* list);

/**
 * @brief 获取下一个结点
 * @param node 结点
 * @return 下一个结点
 */
doubly_linked_list_node_t* doubly_linked_list_next(doubly_linked_list_node_t* node);

/**
 * @brief 获取上一个结点
 * @param node 结点
 * @return 上一个结点
 */
doubly_linked_list_node_t* doubly_linked_list_prev(doubly_linked_list_node_t* node);

// 正向遍历双向链表
#define DOUBLE_LINKED_LIST_FOR_EACH(node, list)                            \
    for (doubly_linked_list_node_t* node = doubly_linked_list_begin(list); \
         node != doubly_linked_list_end(list)->next;                       \
         node = doubly_linked_list_next(node))

// 反向遍历双向链表
#define DOUBLE_LINKED_LIST_REVERSE_FOR_EACH(node, list)                  \
    for (doubly_linked_list_node_t* node = doubly_linked_list_end(list); \
         node != doubly_linked_list_begin(list)->prev;                   \
         node = doubly_linked_list_prev(node))

#endif   // !DOUBLY_LINKED_LIST_H
