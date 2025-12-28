/**
 * @file em_async.h
 * @author GitHub Copilot
 * @brief 轻量级异步工作队列实现，支持裸机与 RTOS 环境。
 * @version 0.1
 * @date 2025-12-28
 */

#ifndef EM_ASYNC_H
#define EM_ASYNC_H

#include "datatype.h"
#include "ringbuffer.h"

/**
 * @brief 异步工作函数指针类型
 * @param arg 用户自定义参数
 */
typedef void (*em_async_work_func_t)(void* arg);

/**
 * @brief 异步工作项结构体
 */
typedef struct {
    em_async_work_func_t func;  // 执行函数
    void*                arg;   // 函数参数
} em_async_work_item_t;

/**
 * @brief 异步执行器上下文
 */
typedef struct {
    ringbuffer_t rb;            // 内部环形缓冲区
    void (*on_notify)(void);    // 提交新任务时的通知回调（用于唤醒 RTOS 任务）
} em_async_t;

/**
 * @brief 初始化异步执行器
 * @param async 执行器句柄
 * @param buffer 存储工作项的内存块 (大小需为 sizeof(em_async_work_item_t) 的倍数)
 * @param size 内存块总大小
 * @param on_notify 通知回调 (可选，RTOS 下可用于发送信号量)
 */
void em_async_init(em_async_t* async, void* buffer, uint32_t size, void (*on_notify)(void));

/**
 * @brief 提交一个异步工作项 (中断安全)
 * @param async 执行器句柄
 * @param func 执行函数
 * @param arg 函数参数
 * @return true 提交成功
 * @return false 队列已满
 */
bool em_async_submit(em_async_t* async, em_async_work_func_t func, void* arg);

/**
 * @brief 执行队列中的所有工作项
 * @note 裸机环境下在 while(1) 中调用；RTOS 环境下在专用任务中调用。
 * @param async 执行器句柄
 * @return uint32_t 本次执行的任务数量
 */
uint32_t em_async_process(em_async_t* async);

/**
 * @brief 获取队列中待处理的任务数量
 */
uint32_t em_async_pending_count(em_async_t* async);

#endif // EM_ASYNC_H
