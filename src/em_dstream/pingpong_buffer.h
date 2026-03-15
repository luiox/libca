/**
 * @file pingpong_buffer.h
 * @author canrad (1517807724@qq.com)
 * @brief 乒乓缓冲区，即双缓冲区，主要是配合 DMA 使用。
 *        乒乓缓冲区的原理是有两个缓冲区，交替使用，当一个缓冲区正在被处理时，另一个缓冲区可以被填充数据。
 *        这样可以减少数据处理的延迟，提高数据传输效率。
 * DMA 场景：
 * DMA 向 write_buffer 填充数据时，调用 pingpong_buf_start_write 设置写入标志
 * DMA 传输完成后，调用 pingpong_buf_end_write 清除标志
 * 应用程序处理 read_buffer 中的数据，处理完成后安全地调用 pingpong_buf_switch 切换缓冲区角色
 * @version 0.1
 * @date 2025-07-27
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef PINGPONG_BUFFER_H
#define PINGPONG_BUFFER_H

#include <em_base/datatype.h> 

// 乒乓缓冲区状态定义
#define PINGPONG_BUF_IDLE 0      // 空闲状态
#define PINGPONG_BUF_WRITING 1   // 正在写入状态

typedef struct
{
    u8* read_buffer; // 读缓冲区，通常用于解析数据
    u8* write_buffer; // 写缓冲区，通常给DMA使用
    usize buffer_size; // 缓冲区大小
    volatile u8 write_flag;   // 写入标志，1表示正在写入，0表示空闲
} pingpong_buffer_t;

// 函数声明
void pingpong_buf_init(pingpong_buffer_t* pingpong_buf, u8* buffer1, u8* buffer2,
                           usize buffer_size);
u8   pingpong_buf_switch(pingpong_buffer_t* pingpong_buf);
u8*  pingpong_buf_get_read_buffer(pingpong_buffer_t* pingpong_buf);
u8*  pingpong_buf_get_write_buffer(pingpong_buffer_t* pingpong_buf);
usize  pingpong_buf_get_size(pingpong_buffer_t* pingpong_buf);
void pingpong_buf_clear(u8* buffer, usize size);
void pingpong_buf_start_write(pingpong_buffer_t* pingpong_buf);
void pingpong_buf_end_write(pingpong_buffer_t* pingpong_buf);
u8   pingpong_buf_is_writing(pingpong_buffer_t* pingpong_buf);

#endif   // PINGPONG_BUFFER_H
