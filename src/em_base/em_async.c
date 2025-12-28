/**
 * @file em_async.c
 * @author GitHub Copilot
 * @brief 轻量级异步工作队列实现
 */

#include "em_async.h"
#include <string.h>

void em_async_init(em_async_t* async, void* buffer, uint32_t size, void (*on_notify)(void)) {
    if (!async || !buffer) return;
    
    ringbuffer_init(&async->rb, (uint8_t*)buffer, size);
    async->on_notify = on_notify;
}

bool em_async_submit(em_async_t* async, em_async_work_func_t func, void* arg) {
    if (!async || !func) return false;

    em_async_work_item_t item;
    item.func = func;
    item.arg = arg;

    // 检查空间是否足够 (sizeof(em_async_work_item_t))
    if (ringbuffer_free(&async->rb) < sizeof(em_async_work_item_t)) {
        return false;
    }

    // 写入队列
    // 注意：如果多处同时 submit (如多个中断或任务)，此处需要临界区保护
    // 但为了保持库的独立性，建议用户在外部调用时处理，或在 ringbuffer 层处理
    uint32_t written = ringbuffer_write(&async->rb, (uint8_t*)&item, sizeof(em_async_work_item_t));
    
    if (written == sizeof(em_async_work_item_t)) {
        if (async->on_notify) {
            async->on_notify();
        }
        return true;
    }

    return false;
}

uint32_t em_async_process(em_async_t* async) {
    if (!async) return 0;

    uint32_t processed_count = 0;
    em_async_work_item_t item;

    // 循环处理当前队列中的所有任务
    while (ringbuffer_used(&async->rb) >= sizeof(em_async_work_item_t)) {
        if (ringbuffer_read(&async->rb, (uint8_t*)&item, sizeof(em_async_work_item_t)) 
            == sizeof(em_async_work_item_t)) {
            
            if (item.func) {
                item.func(item.arg);
                processed_count++;
            }
        }
    }

    return processed_count;
}

uint32_t em_async_pending_count(em_async_t* async) {
    if (!async) return 0;
    return ringbuffer_used(&async->rb) / sizeof(em_async_work_item_t);
}
