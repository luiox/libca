/* em_dstream.h */
#ifndef EM_DSTREAM_H
#define EM_DSTREAM_H

#include <stdint.h>
#include <stddef.h>

// 前向声明
typedef struct em_dstream em_dstream_t;

// 虚函数表
typedef struct em_dstream_ops {
    size_t (*read)(em_dstream_t* self, void* dst, size_t len);
    int (*peek)(em_dstream_t* self, size_t offset);
    const void* (*get_contiguous)(em_dstream_t* self, size_t* out_len);
    void (*skip)(em_dstream_t* self, size_t len);
} em_dstream_ops_t;

// 核心对象：这就是你的“绑定中间件”
struct em_dstream {
    void* buf_obj;              // 指向底层具体缓冲区
    const em_dstream_ops_t* ops;// 操作函数指针表
    
    size_t cursor;              // 当前流读取位置
    size_t total_len;           // 当前可读数据总量
    
    void* front_ctx;            // 前端上下文
    void* back_ctx;             // 后端上下文
};

/* 标准初始化与绑定 API */
void em_ds_init(em_dstream_t* stream);
void em_ds_bind(em_dstream_t* stream, void* buf_obj, const em_dstream_ops_t* ops);

/* 便捷操作宏 */
#define EM_DS_READ(s, ptr, len) ((s)->ops->read((s), (ptr), (len)))
#define EM_DS_PEEK(s, off)     ((s)->ops->peek((s), (off)))

#endif
