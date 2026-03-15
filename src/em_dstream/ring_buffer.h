/**
 * @file ring_buffer.h
 * @author canrad (1517807724@qq.com)
 * @brief 一个简单的环形缓冲区实现
 *
 * @version 0.3
 * @date   2024.05.31
 * @update 0.2 2026-02-11 完善代码和注释
 * @update 0.3 2026-03-15 根据标准优化和规范代码
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_DSTREAM_RING_BUFFER_H
#define LIBCA_EM_DSTREAM_RING_BUFFER_H

#include <em_base/datatype.h>

// position_size_t 已弃用，直接使用 usize 类型
// typedef usize position_size_t;


// 缓冲区可以选择的大小
#define RINGBUFFER_SIZE_GEN(n) (1 << n)

// 环形缓冲区结构体
typedef struct ring_buffer
{
    u8*       buffer;   // 缓冲区，要求是2的幂次方
    usize          size;     // 缓冲区大小
    usize          used;     // 已使用的大小
    volatile usize read;     // 读指针的位置
    volatile usize write;    // 写指针的位置
} ring_buffer_t;

/**
 * @brief 初始化环形缓冲区.
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区，要求是可用的内存，且大小为2的幂次方
 * @param size 缓冲区大小
 */
void ring_buf_init(ring_buffer_t* rb, u8* buffer, usize size);

/**
 * @brief 重置环形缓冲区.
 * @param rb 环形缓冲区指针
 */
void ring_buf_reset(ring_buffer_t* rb);

/**
 * @brief 往环形缓冲区里写数据.
 * @param rb 环形缓冲区指针
 * @param data 指向数据的指针
 * @param size 期望写入的数据大小
 * @return position_size_t 实际写入的数据大小
 */
usize ring_buf_write(ring_buffer_t* rb, const u8* data, usize size);

/**
 * @brief 从环形缓冲区里读数据.
 * @param rb 环形缓冲区指针
 * @param buf 指向读取缓冲区的指针
 * @param size 期望读取的数据大小
 * @return position_size_t 实际读取的数据大小
 */
usize ring_buf_read(ring_buffer_t* rb, u8* buf, usize size);

/**
 * @brief 预览环形缓冲区里的数据（不弹出）.
 * @param rb 环形缓冲区指针
 * @param buf 指向读取缓冲区的指针
 * @param size 期望预览的数据大小
 * @return position_size_t 实际预览的数据大小
 */
usize ring_buf_peek(const ring_buffer_t* rb, u8* buf, usize size);

/**
 * @brief 跳过（丢弃）环形缓冲区里的数据.
 * @param rb 环形缓冲区指针
 * @param size 期望跳过的数据大小
 * @return position_size_t 实际跳过的数据大小
 */
usize ring_buf_skip(ring_buffer_t* rb, usize size);

/**
 * @brief 获取环形缓冲区里的数据大小.
 * @param rb 环形缓冲区指针
 * @return position_size_t 已使用的大小
 */
usize ring_buf_used(const ring_buffer_t* rb);

/**
 * @brief 获取环形缓冲区里的空闲大小.
 * @param rb 环形缓冲区指针
 * @return position_size_t 空闲大小
 */
usize ring_buf_free(const ring_buffer_t* rb);


// 工具函数

// 读取单个字节
u8 ring_buf_read_u8(ring_buffer_t* rb);
// 写入单个字节
void ring_buf_write_u8(ring_buffer_t* rb, u8 value);

// 读取 16 位整数
i16 ring_buf_read_i16(ring_buffer_t* rb);
u16 ring_buf_read_u16(ring_buffer_t* rb);
// 写入 16 位整数
void ring_buf_write_i16(ring_buffer_t* rb, i16 value);
void ring_buf_write_u16(ring_buffer_t* rb, u16 value);

// 读取 32 位整数
i32 ring_buf_read_i32(ring_buffer_t* rb);
u32 ring_buf_read_u32(ring_buffer_t* rb);
// 写入 32 位整数
void ring_buf_write_i32(ring_buffer_t* rb, i32 value);
void ring_buf_write_u32(ring_buffer_t* rb, u32 value);

// 读取浮点数
float ring_buf_read_float(ring_buffer_t* rb);
// 写入浮点数
void ring_buf_write_float(ring_buffer_t* rb, float value);

// 计算缓冲区中所有数据的校验和
u8 ring_buf_calculate_checksum(const ring_buffer_t* rb);

#endif   // !LIBCA_EM_DSTREAM_RING_BUFFER_H
