/**
 * @file ringbuffer_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 环形缓冲区工具函数，提供对基本数据类型的读写支持
 * @version 0.1
 * @date 2025-12-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef LIBCA_EM_UTIL_RINGBUFFER_UTIL_H
#define LIBCA_EM_UTIL_RINGBUFFER_UTIL_H

#include "../em_base/datatype.h"
#include "../em_base/ringbuffer.h"

// 读取单个字节
u8 ringbuffer_read_u8(ringbuffer_t* rb);
// 写入单个字节
void ringbuffer_write_u8(ringbuffer_t* rb, u8 value);

// 读取 16 位整数
i16 ringbuffer_read_i16(ringbuffer_t* rb);
u16 ringbuffer_read_u16(ringbuffer_t* rb);
// 写入 16 位整数
void ringbuffer_write_i16(ringbuffer_t* rb, i16 value);
void ringbuffer_write_u16(ringbuffer_t* rb, u16 value);

// 读取 32 位整数
i32 ringbuffer_read_i32(ringbuffer_t* rb);
u32 ringbuffer_read_u32(ringbuffer_t* rb);
// 写入 32 位整数
void ringbuffer_write_i32(ringbuffer_t* rb, i32 value);
void ringbuffer_write_u32(ringbuffer_t* rb, u32 value);

// 读取浮点数
float ringbuffer_read_float(ringbuffer_t* rb);
// 写入浮点数
void ringbuffer_write_float(ringbuffer_t* rb, float value);

// 计算缓冲区中所有数据的校验和
u8 ringbuffer_calculate_checksum(const ringbuffer_t* rb);

#endif // LIBCA_EM_UTIL_RINGBUFFER_UTIL_H
