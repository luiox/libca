/**
 * @file async.h
 * @author canrad (1517807724@qq.com)
 * @brief 轻量级异步工作队列实现，基于轮询机制
 * 注意：仅适用于单核MCU环境
 * @version 0.1
 * @date 2026-01-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_BASE_ASYNC_H
#define LIBCA_EM_BASE_ASYNC_H

#include "datatype.h"

/**
 * @brief 任务函数指针类型
 */
typedef void (*task_item_fn_t)(void *arg);

/**
 * @brief 任务项结构体
 */
typedef struct task_item {
    task_item_fn_t func;
    void *arg;
} task_item_t;

/**
 * @brief 异步工作队列控制块
 */
typedef struct async {
    task_item_t *buffer;    /**< 任务缓冲区指针 */
    usize size;             /**< 缓冲区大小 (必须是2的幂) */
    volatile usize head;    /**< 写索引 */
    volatile usize tail;    /**< 读索引 */
} async_t;

/**
 * @brief 初始化异步工作队列
 * 
 * @param self 队列对象指针
 * @param buffer 用户提供的任务缓冲区数组
 * @param size 缓冲区大小 (必须是2的幂，例如 4, 8, 16, 32...)
 * @return true 初始化成功
 * @return false 初始化失败 (size不是2的幂或指针为空)
 */
bool async_init(async_t *self, task_item_t *buffer, usize size);

/**
 * @brief 提交一个异步任务
 * 
 * @param self 队列对象指针
 * @param func 任务函数
 * @param arg 任务参数
 * @return true 提交成功
 * @return false 提交失败 (队列已满)
 */
bool async_submit(async_t *self, task_item_fn_t func, void *arg);

/**
 * @brief 轮询执行挂起的任务
 * 
 * @note 该函数应在主循环中调用。每次调用会执行所有当前挂起的任务。
 * 
 * @param self 队列对象指针
 */
void async_poll(async_t *self);

#endif // LIBCA_EM_BASE_ASYNC_H
