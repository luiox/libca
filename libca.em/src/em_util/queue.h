/// @file   doubly_linked_list.h
/// @brief  一个简单的双向链表实现
///
/// @author Canrad
/// @date   2024.06.02

#ifndef QUEUE_H
#define QUEUE_H

// 注意这个队列是基于doubly_linked_list_t实现的，而且使用了malloc和free用于创建节点

#include <stdint.h>
#include <stdbool.h>

#include "doubly_linked_list.h"
typedef doubly_linked_list_t queue_t;

// 初始化队列
void queue_init(queue_t* queue);

// 加入一个元素到对位
void queue_push(queue_t* queue, void* data);

// 弹出队头元素
void queue_pop(queue_t* queue);

// 获取队头元素
void* queue_front(queue_t* queue);

// 获取队列大小
int32_t queue_size(queue_t* queue);

// 判断队列是否为空
bool queue_empty(queue_t* queue);

// 清空队列
void queue_clear(queue_t* queue);





#endif   // !QUEUE_H
