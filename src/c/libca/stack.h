/*********************************************************************
 * @file   stack.h
 * @brief  一个简单的栈实现
 *
 * @author Canrad
 * @date   2024.06.02
 *********************************************************************/
#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include <stdbool.h>

// 栈的结构类似下面这样
// -----------高地址（bottom+size）---------
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// -------------当前栈顶（top）--------------
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// |                                      |
// ---------------低（bottom）--------------

typedef struct stack
{
    uint8_t* top;        // 顶端
    uint8_t* bottom;     // 底端
    int32_t  size;       // 栈中使用的字节数
    int32_t  capacity;   // 栈的容量
} stack_t;

/**
 * @brief 初始化栈
 * @param stack 栈指针
 * @param buf 栈缓冲区
 * @param size 栈缓冲区大小
 */
void stack_init(stack_t* s, void* buf, int32_t size);

/**
 * @brief 压栈
 * @param stack 栈指针
 * @param data 压入的数据
 * @param size 数据大小
 * @return 如果栈的剩余空间不足以容纳数据，则不会执行该操作，返回false，否则操作成功返回true
 */
bool stack_push(stack_t* s, void* data, int32_t size);

/**
 * @brief 弹出栈顶的数据
 * @param stack 栈指针
 * @param size 弹出数据的大小
 * @return 如果栈的剩余空间不足，则不会执行该操作，且返回false，否则操作成功返回true
 */
bool stack_pop(stack_t* s, int32_t size);

/**
 * @brief 获取栈顶数据
 * @param stack 栈指针
 * @param buf 存储数据的缓冲区，要保证缓冲区够大
 * @param size 数据大小
 */
uint8_t stack_peek(stack_t* s, void* buf, int32_t size);

/**
 * @brief 检查栈是否为空
 * @param stack 栈指针
 * @return 栈是否为空
 */
bool stack_empty(stack_t* s);

/**
 * @brief 检查栈是否已满
 * @param stack 栈指针
 * @return 栈是否已满
 */
bool stack_full(stack_t* s);

#endif   // !STACK_H
