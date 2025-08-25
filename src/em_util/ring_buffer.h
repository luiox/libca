/*

https://github.com/dhess/c-ringbuf/blob/master/ringbuf.c

环形队列实现原理

https://www.cnblogs.com/shihuvini/p/8538732.html


*/

#ifndef MYLIB_UTILITY_RING_BUFFER_H
#define MYLIB_UTILITY_RING_BUFFER_H


#include <stdint.h>

// 环形队列结构体
typedef struct
{
    uint8_t* buffer;   // 缓冲区
    uint16_t head;     // 队首指针
    uint16_t tail;     // 队尾指针
    uint16_t size;     // 环形队列的尺寸
} ringbuffer_t;

// 创建队列
void ringbuffer_init(ringbuffer_t* ringBuf, uint16_t capacity, uint8_t* buffer);
// 队列重置
void ringbuffer_reset(ringbuffer_t* ringBuf);
// 销毁队列
void ringBuffer_destory(ringbuffer_t* ringBuf);
// 获取队列的容量
uint16_t ringbuffer_get_capacity(ringbuffer_t* ringBuf);
// 获取环形队列已经存好的字节数
uint16_t ringbuffer_get_used_bytes_size(ringbuffer_t* ringBuf);
// 获取队列的剩余的空闲字节
uint16_t ringbuffer_get_free_bytes_size(ringbuffer_t* ringBuf);
// 队列是否为空
uint8_t ringbuffer_is_empty(ringbuffer_t* ringBuf);
// 队列是否已满
uint8_t ringbuffer_is_full(ringbuffer_t* ringBuf);
// 根据索引号读取第i个元素
uint8_t ringbuffer_get_value_by_index(ringbuffer_t* ringBuf, uint16_t index);
// 弹出队首元素
uint8_t ringbuffer_pop(ringbuffer_t* ringBuf);
// 读取单个字节
uint8_t ringbuffer_read_byte(ringbuffer_t* ringBuf);
// 读取字节数组
void ringbuffer_read_byte_array(ringbuffer_t* ringBuf, uint8_t* dest, uint16_t size);
// 读取有符号Short整数(两个字节)
int16_t ringbuffer_read_short(ringbuffer_t* ringBuf);
// 读取无符号Short整数(两个字节)
uint16_t ringbuffer_read_ushort(ringbuffer_t* ringBuf);
// 读取有符号Long类型的整数(四个字节)
int32_t ringbuffer_read_long(ringbuffer_t* ringBuf);
// 读取无符号Long类型的整数(四个字节)
uint32_t ringbuffer_read_ulong(ringbuffer_t* ringBuf);
// 读取浮点数(四个字节)
float ringbuffer_read_float(ringbuffer_t* ringBuf);
// 写入队尾元素
void ringbuffer_push(ringbuffer_t* ringBuf, uint8_t value);
// 写入单个字节
void ringbuffer_write_byte(ringbuffer_t* ringBuf, uint8_t value);
// 写入字节数组
void ringbuffer_write_byte_array(ringbuffer_t* ringBuf, uint8_t* src, uint16_t size);
// 写入有符号Short整数(两个字节)
void ringbuffer_write_short(ringbuffer_t* ringBuf, int16_t value);
// 写入无符号Short整数(两个字节)
void ringbuffer_write_ushort(ringbuffer_t* ringBuf, uint16_t value);
// 写入有符号Long类型的整数(四个字节)
void ringbuffer_write_long(ringbuffer_t* ringBuf, int32_t value);
// 写入无符号Long类型的整数(四个字节)
void ringbuffer_write_ulong(ringbuffer_t* ringBuf, uint32_t value);
// 写入浮点数(四个字节)
void ringbuffer_write_float(ringbuffer_t* ringBuf, float value);
// 计算缓冲区所有自己的校验和()
uint8_t ringbuffer_get_checksum(ringbuffer_t* ringBuf);

#endif   // !MYLIB_BASE_RING_BUFFER_H
