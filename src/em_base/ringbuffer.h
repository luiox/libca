/*********************************************************************
 * @file   ringbuffer.h
 * @brief  一个简单的环形缓冲区实现
 *
 * @author Canrad
 * @date   2024.05.31
 *********************************************************************/

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

// 如果定义了USE_LESS_MEMORY，则使用uint16_t作为位置大小类型，否则使用uint32_t
#ifdef USE_LESS_MEMORY
typedef uint16_t position_size_t;
#else
typedef uint32_t position_size_t;
#endif   // USE_LESS_MEMORY

// 缓冲区可以选择的大小
#define RINGBUFFER_SIZE_GEN(n) (1 << n)

// 环形缓冲区结构体
typedef struct
{
    uint8_t*       buffer;   // 缓冲区，要求是2的幂次方
    position_size_t size;     // 缓冲区大小
    position_size_t used;     // 已使用的大小
    position_size_t read;     // 读指针的位置
    position_size_t write;    // 写指针的位置
} ringbuffer_t;

/**
 * @brief 初始化环形缓冲区.
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区，要求是可用的内存，且大小为2的幂次方
 * @param size 缓冲区大小
 */
void ringbuffer_init(ringbuffer_t* rb, uint8_t* buffer, position_size_t size);

/**
 * @brief 重置环形缓冲区.
 * @param rb 环形缓冲区指针
 */
void ringbuffer_reset(ringbuffer_t* rb);

/**
 * @brief 往环形缓冲区里写数据.
 * @param rb 环形缓冲区指针
 * @param data 指向数据的指针
 * @param size 期望写入的数据大小
 * @return position_size_t 实际写入的数据大小
 */
position_size_t ringbuffer_write(ringbuffer_t* rb, const uint8_t* data, position_size_t size);

/**
 * @brief 从环形缓冲区里读数据.
 * @param rb 环形缓冲区指针
 * @param buf 指向读取缓冲区的指针
 * @param size 期望读取的数据大小
 * @return position_size_t 实际读取的数据大小
 */
position_size_t ringbuffer_read(ringbuffer_t* rb, uint8_t* buf, position_size_t size);

/**
 * @brief 预览环形缓冲区里的数据（不弹出）.
 * @param rb 环形缓冲区指针
 * @param buf 指向读取缓冲区的指针
 * @param size 期望预览的数据大小
 * @return position_size_t 实际预览的数据大小
 */
position_size_t ringbuffer_peek(const ringbuffer_t* rb, uint8_t* buf, position_size_t size);

/**
 * @brief 跳过（丢弃）环形缓冲区里的数据.
 * @param rb 环形缓冲区指针
 * @param size 期望跳过的数据大小
 * @return position_size_t 实际跳过的数据大小
 */
position_size_t ringbuffer_skip(ringbuffer_t* rb, position_size_t size);

/**
 * @brief 获取环形缓冲区里的数据大小.
 * @param rb 环形缓冲区指针
 * @return position_size_t 已使用的大小
 */
position_size_t ringbuffer_used(const ringbuffer_t* rb);

/**
 * @brief 获取环形缓冲区里的空闲大小.
 * @param rb 环形缓冲区指针
 * @return position_size_t 空闲大小
 */
position_size_t ringbuffer_free(const ringbuffer_t* rb);

#endif   // !RINGBUFFER_H
