#include "async.h"

/// @brief 检查是否为2的幂
static inline bool is_power_of_two(usize n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

bool async_init(async_t *self, task_item_t *buffer, usize size) {
    if (!self || !buffer || !is_power_of_two(size)) {
        return false;
    }

    self->buffer = buffer;
    self->size = size;
    self->head = 0;
    self->tail = 0;

    return true;
}

bool async_submit(async_t *self, task_item_fn_t func, void *arg) {
    if (!self || !func) {
        return false;
    }

    usize next_head = (self->head + 1) & (self->size - 1);

    // 检查队列是否已满 (next_head == tail 表示满，留一个空位)
    if (next_head == self->tail) {
        return false;
    }

    // 写入任务
    self->buffer[self->head].func = func;
    self->buffer[self->head].arg = arg;

    // 更新写索引 (使用内存屏障确保数据写入完成后才更新索引，但在简单MCU上volatile通常足够)
    // 这里依赖 volatile 的顺序性
    self->head = next_head;

    return true;
}

void async_poll(async_t *self) {
    if (!self) {
        return;
    }

    // 循环处理直到队列为空
    while (self->head != self->tail) {
        // 读取任务
        task_item_t task = self->buffer[self->tail];
        
        // 更新读索引
        self->tail = (self->tail + 1) & (self->size - 1);

        // 执行任务
        if (task.func) {
            task.func(task.arg);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                                 Unit Tests                                 */
/* -------------------------------------------------------------------------- */
