/// @file lifo.h
/// @author Canrad
/// @brief 侵入式 LIFO（后进先出/栈）head-only 实现
/// @version 0.1
/// @date 2026-03-03
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_UTIL_LIFO_H
#define LIBCA_EM_UTIL_LIFO_H

#include <em_base/datatype.h>
#include <em_base/macro_util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lifo_node
{
    struct lifo_node* next;
} lifo_node_t;

typedef struct lifo
{
    lifo_node_t* top;
    usize        size;
} lifo_t;

/// @brief 初始化 LIFO 容器
/// @param self LIFO 对象
static inline void lifo_init(lifo_t* self)
{
    self->top  = NULL;
    self->size = 0;
}

/// @brief 判断 LIFO 是否为空
/// @param self LIFO 对象
/// @return true 空，false 非空
static inline bool lifo_is_empty(const lifo_t* self)
{
    return self->top == NULL;
}

/// @brief 向 LIFO 压入节点（O(1)）
/// @param self LIFO 对象
/// @param node 节点指针
static inline void lifo_push(lifo_t* self, lifo_node_t* node)
{
    node->next = self->top;
    self->top  = node;
    self->size++;
}

/// @brief 从 LIFO 弹出节点（O(1)）
/// @param self LIFO 对象
/// @return 弹出的节点；若为空返回 NULL
static inline lifo_node_t* lifo_pop(lifo_t* self)
{
    lifo_node_t* node = self->top;
    if (node != NULL) {
        self->top  = node->next;
        node->next = NULL;
        self->size--;
    }
    return node;
}

/// @brief 查看栈顶节点但不弹出
/// @param self LIFO 对象
/// @return 栈顶节点；若为空返回 NULL
static inline lifo_node_t* lifo_peek(const lifo_t* self)
{
    return self->top;
}

/// @brief 获取当前节点数
/// @param self LIFO 对象
/// @return 当前节点数
static inline usize lifo_size(const lifo_t* self)
{
    return self->size;
}

/// @brief 从节点指针获取宿主结构体指针
#define lifo_entry(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_UTIL_LIFO_H
