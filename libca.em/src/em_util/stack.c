#include "stack.h"
#include <assert.h>
#include <string.h>

/// @brief 初始化栈
/// @param stack 栈指针
/// @param buf 栈缓冲区
/// @param capacity 栈缓冲区大小
void stack_init(stack_t* s, void* buf, int32_t size)
{
    assert(s);
    assert(buf);

    s->capacity = size;
    s->bottom   = buf;
    s->top      = buf;
    s->size     = 0;
}

/// @brief 压栈
/// @param stack 栈指针
/// @param data 压入的数据
/// @param size 数据大小
/// @return 如果栈的剩余空间不足以容纳数据，则不会执行该操作，返回false，否则操作成功返回true
bool stack_push(stack_t* s, void* data, int32_t size)
{
    assert(s);
    assert(data);

    if (s->size + size > s->capacity) {
        // 剩余空间不足以存放数据
        return false;
    }
    // 压栈
    memcpy((char*)s->top + s->size, data, size);
    s->size += size;

    return true;
}

/// @brief 弹出栈顶的数据
/// @param stack 栈指针
/// @param size 弹出数据的大小
/// @return 如果栈的剩余空间不足，则不会执行该操作，且返回false，否则操作成功返回true
bool stack_pop(stack_t* s, int32_t size)
{
    assert(s);

    if (s->size < size) {
        // 栈的剩余空间不足
        return false;
    }
    // 弹出栈顶数据
    s->size -= size;

    return true;
}

/// @brief 获取栈顶数据
/// @param stack 栈指针
/// @param buf 存储数据的缓冲区，要保证缓冲区够大
/// @param size 数据大小
uint8_t stack_peek(stack_t* s, void* buf, int32_t size)
{
    assert(s);
    assert(buf);

    if (s->size < size) {
        // 栈的剩余空间不足
        return false;
    }
    // 获取栈顶数据
    memcpy(buf, (char*)s->top + s->size - size, size);

    return true;
}

/// @brief 检查栈是否为空
/// @param stack 栈指针
/// @return 栈是否为空
bool stack_empty(stack_t* s)
{
    assert(s);

    return s->size == 0;
}

/// @brief 检查栈是否已满
/// @param stack 栈指针
/// @return 栈是否已满
bool stack_full(stack_t* s)
{
    assert(s);

    return s->size == s->capacity;
}
