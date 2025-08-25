/**
 * @file ping_pong_buffer.h
 * @author canrad (1517807724@qq.com)
 * @brief 乒乓缓冲区，即双缓冲区，主要是配合DMA使用
 *        乒乓缓冲区的原理是有两个缓冲区，交替使用，
 *        当一个缓冲区正在被处理时，另一个缓冲区可以被填充数据。
 *        这样可以减少数据处理的延迟，提高数据传输效率。
 * DMA的情况：
 * DMA向 write_buffer 填充数据时，调用 ping_pong_buffer_start_write 设置写入标志
 * DMA传输完成后，调用 ping_pong_buffer_end_write 清除标志
 * 应用程序处理 read_buffer 中的数据
 * 处理完成后，安全地调用 ping_pong_buffer_switch 切换缓冲区角色
 * @version 0.1
 * @date 2025-07-27
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef PING_PONG_BUFFER_H
#define PING_PONG_BUFFER_H

#include "../base/datatype.h" 

// 乒乓缓冲区状态定义
#define PING_PONG_BUFFER_IDLE 0      // 空闲状态
#define PING_PONG_BUFFER_WRITING 1   // 正在写入状态

typedef struct
{
    u8* read_buffer; // 读缓冲区，通常用于解析数据
    u8* write_buffer; // 写缓冲区，通常给DMA使用
    usize buffer_size; // 缓冲区大小
    vu8 write_flag;   // 写入标志，1表示正在写入，0表示空闲
} ping_pong_buffer_t;

// 函数声明
void ping_pong_buffer_init(ping_pong_buffer_t* ping_pong_buf, u8* buffer1, u8* buffer2,
                           usize buffer_size);
u8   ping_pong_buffer_switch(ping_pong_buffer_t* ping_pong_buf);
u8*  ping_pong_buffer_get_read_buffer(ping_pong_buffer_t* ping_pong_buf);
u8*  ping_pong_buffer_get_write_buffer(ping_pong_buffer_t* ping_pong_buf);
usize  ping_pong_buffer_get_size(ping_pong_buffer_t* ping_pong_buf);
void ping_pong_buffer_clear(u8* buffer, usize size);
void ping_pong_buffer_start_write(ping_pong_buffer_t* ping_pong_buf);
void ping_pong_buffer_end_write(ping_pong_buffer_t* ping_pong_buf);
u8   ping_pong_buffer_is_writing(ping_pong_buffer_t* ping_pong_buf);

#endif   // PING_PONG_BUFFER_H
