/**
 * @file file_transfer.h
 * @author canrad (1517807724@qq.com)
 * @brief 定义文件传输协议相关的接口
 * @version 0.1
 * @date 2025-12-27
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef LIBCA_EM_PROTOCOL_FILE_TRANSFER_H
#define LIBCA_EM_PROTOCOL_FILE_TRANSFER_H

#include "../em_base/datatype.h"

typedef void* cbs_t;

// 文件传输协议接口
typedef struct {
    // 1. 核心处理：输入字节流
    // in_buf: 串口收到的数据
    // in_len: 数据长度
    // 返回值: >0 表示处理后生成了需要回复的数据长度，0 表示无回复
    i32 (*process)(void *owner, const u8 *in_buf, usize in_len);

    // 2. 核心处理：输出字节流
    // out_buf: 用于存放待发送数据的缓冲区
    // out_len: 缓冲区大小
    // 返回值: 实际填入的数据长度 (如果为0，表示暂时不需要发数据)
    i32 (*poll)(void *owner, u8 *out_buf, usize out_len);
    
    // 3. 回调集
    cbs_t cbs;
} file_transfer_ops_t;


#endif // !LIBCA_EM_PROTOCOL_FILE_TRANSFER_H
