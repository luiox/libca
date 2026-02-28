# em_dstream 使用文档

## 目录

- [快速开始](#快速开始)
- [dstream 核心接口](#dstream-核心接口)
- [定界符解析器](#定界符解析器)
- [长度前置解析器](#长度前置解析器)
- [常见用例](#常见用例)
- [常见问题](#常见问题)
- [编译运行](#编译运行)

---

## 快速开始

### 5 分钟上手

#### 第 1 步：实现 dstream_t 的底层缓冲区

```c
#include "dstream.h"

/* 示例：简单的环形缓冲区实现 */
typedef struct {
    u8 buffer[256];
    usize head;
    usize tail;
    usize capacity;
} ring_buffer_t;

/* 实现 dstream_ops 接口 */
static usize ring_capacity(dstream_t* self) {
    ring_buffer_t* rb = (ring_buffer_t*)self->buf_obj;
    return rb->capacity;
}

static usize ring_used(dstream_t* self) {
    ring_buffer_t* rb = (ring_buffer_t*)self->buf_obj;
    return (rb->head - rb->tail) % rb->capacity;
}

static i32 ring_peek(dstream_t* self, usize offset, void* dest, usize len) {
    ring_buffer_t* rb = (ring_buffer_t*)self->buf_obj;
    /* ... 实现peek逻辑 ... */
    return (i32)actual;
}

/* 其他接口实现... */

static dstream_ops_t ring_ops = {
    .capacity = ring_capacity,
    .used = ring_used,
    .peek = ring_peek,
    .skip = ring_skip,
    /* ... */
};
```

#### 第 2 步：使用定界符解析器解析 AT 指令

```c
#include "delimiter_parser.h"

ring_buffer_t uart_rx_buffer;
dstream_t ds = { .buf_obj = &uart_rx_buffer, .ops = &ring_ops };

delimiter_parser_t parser;
delimiter_parser_init(&parser, &ds, NULL, 0, (u8*)"\r\n", 2, 128);

/* 在串口中断中写入数据 */
void USART1_IRQHandler(void) {
    u8 ch = USART_ReceiveData(USART1);
    /* 写入环形缓冲区... */
}

/* 在主循环中解析 */
void process_frames(void) {
    usize len;
    if (delimiter_parser_get_frame(&parser, &len) == DELIMITER_PARSER_OK) {
        u8 frame[128];
        dstream_peek(&ds, 0, frame, len);
        /* 处理 AT 指令 */
        delimiter_parser_consume(&parser);
    }
}
```

#### 第 3 步：使用长度前置解析器解析自定义协议

```c
#include "length_parser.h"

/* 校验函数 */
static u8 my_checksum(const u8* data, usize len, u8 prev) {
    u8 sum = prev;
    for (usize i = 0; i < len; i++) sum += data[i];
    return sum;
}

length_parser_t parser;
length_parser_cksum_func_t cksum = { .checksum_u8 = my_checksum };
length_parser_init(&parser, &ds, NULL, 0, 2, false, 1,
                   LENGTH_PARSER_CKSUM_U8, cksum, 0, 256);

usize data_len;
if (length_parser_get_frame(&parser, &data_len) == LENGTH_PARSER_OK) {
    u8 data[256];
    dstream_peek(&ds, 2, data, data_len);  /* 跳过长度字段 */
    /* 处理数据 */
    length_parser_consume(&parser);
}
```

---

## dstream 核心接口

### 数据流抽象

`dstream_t` 是一个通用的数据流抽象，屏蔽底层缓冲区实现差异：

```c
typedef struct dstream {
    void* buf_obj;              /* 底层缓冲区对象 */
    const dstream_ops_t* ops;   /* 操作函数指针表 */
} dstream_t;
```

### 核心 API

```c
/* 容量查询 */
usize dstream_capacity(dstream_t* self);  /* 总容量 */
usize dstream_used(dstream_t* self);       /* 已使用长度 */
usize dstream_available(dstream_t* self);  /* 剩余空间 */

/* 游标操作 */
void dstream_skip(dstream_t* self, usize len);    /* 跳过 n 字节 */
void dstream_rewind(dstream_t* self, usize len);  /* 回退 n 字节 */
usize dstream_offset(dstream_t* self);            /* 当前偏移 */
bool dstream_reset(dstream_t* self, usize pos);   /* 设置位置 */

/* 数据读写 */
i32 dstream_read(dstream_t* self, void* dest, usize len);
i32 dstream_peek(dstream_t* self, usize offset, void* dest, usize len);
i32 dstream_write(dstream_t* self, const void* src, usize len);
```

### 辅助接口（字节序处理）

```c
/* Peek 系列不移动 cursor */
u8  dstream_peek_u8(dstream_t* self, usize offset);
u16 dstream_peek_u16_le(dstream_t* self, usize offset);
u16 dstream_peek_u16_be(dstream_t* self, usize offset);
u32 dstream_peek_u32_le(dstream_t* self, usize offset);
u32 dstream_peek_u32_be(dstream_t* self, usize offset);

/* Read 系列移动 cursor */
u8  dstream_read_u8(dstream_t* self);
u16 dstream_read_u16_le(dstream_t* self);
u16 dstream_read_u16_be(dstream_t* self);
u32 dstream_read_u32_le(dstream_t* self);
u32 dstream_read_u32_be(dstream_t* self);

/* Write 系列 */
i32 dstream_write_u8(dstream_t* self, u8 value);
i32 dstream_write_u16_le(dstream_t* self, u16 value);
i32 dstream_write_u16_be(dstream_t* self, u16 value);
i32 dstream_write_u32_le(dstream_t* self, u32 value);
i32 dstream_write_u32_be(dstream_t* self, u32 value);
```

---

## 定界符解析器

### API 参考

```c
/**
 * @brief 初始化定界符解析器
 * @param self        解析器对象
 * @param ds          数据流
 * @param header      头部定界符（可为NULL，此时必须提供尾部）
 * @param header_len  头部长度
 * @param trailer     尾部定界符（可为NULL，此时必须提供头部）
 * @param trailer_len 尾部长度
 * @param max_frame_len 最大允许帧长度
 */
void delimiter_parser_init(delimiter_parser_t* self, dstream_t* ds,
                           const u8* header, usize header_len,
                           const u8* trailer, usize trailer_len,
                           usize max_frame_len);

/**
 * @brief 尝试获取一个完整数据帧
 * @return DELIMITER_PARSER_OK         成功
 * @return DELIMITER_PARSER_NEED_MORE  数据不足
 * @return DELIMITER_PARSER_ERR_FRAME_TOO_LONG  帧超长
 */
delimiter_parser_result_t delimiter_parser_get_frame(delimiter_parser_t* self, usize* out_len);

/**
 * @brief 消费当前帧，移动 cursor
 */
void delimiter_parser_consume(delimiter_parser_t* self);

/**
 * @brief 重置解析器状态
 */
void delimiter_parser_reset(delimiter_parser_t* self);
```

### 使用示例

#### 仅尾部定界（AT 指令）

```c
delimiter_parser_t parser;
u8 trailer[] = "\r\n";
delimiter_parser_init(&parser, &ds, NULL, 0, trailer, 2, 256);

usize len;
if (delimiter_parser_get_frame(&parser, &len) == DELIMITER_PARSER_OK) {
    /* len 包含尾部 "\r\n" */
    u8 cmd[256];
    dstream_peek(&ds, 0, cmd, len);
    /* cmd 内容如 "AT+RST\r\n" */
    delimiter_parser_consume(&parser);
}
```

#### 仅头部定界

```c
delimiter_parser_t parser;
u8 header[] = {0x55, 0xAA};
delimiter_parser_init(&parser, &ds, header, 2, NULL, 0, 128);

/* 注意：无尾部时帧无法自动结束，需配合超时机制 */
usize len;
delimiter_parser_result_t ret = delimiter_parser_get_frame(&parser, &len);
if (ret == DELIMITER_PARSER_NEED_MORE) {
    /* 持续等待数据，或通过超时判断帧结束 */
}
```

#### 头尾定界（HDLC 风格）

```c
delimiter_parser_t parser;
u8 delim[] = {0x7E};
delimiter_parser_init(&parser, &ds, delim, 1, delim, 1, 64);

usize len;
if (delimiter_parser_get_frame(&parser, &len) == DELIMITER_PARSER_OK) {
    /* len 包含头尾的 0x7E */
}
```

---

## 长度前置解析器

### API 参考

```c
/**
 * @brief 初始化长度前置解析器
 * @param self            解析器对象
 * @param ds              数据流
 * @param header          可选帧头（NULL 表示无头）
 * @param header_len      帧头长度
 * @param len_field_size  长度字段字节数 (1/2/4)
 * @param len_big_endian  长度字段字节序
 * @param checksum_size   校验字段字节数 (0/1/2/4)
 * @param cksum_type      校验类型
 * @param cksum_func      校验函数
 * @param cksum_init_val  校验初始值
 * @param max_frame_len   数据部分最大允许长度
 */
void length_parser_init(length_parser_t* self, dstream_t* ds,
                        const u8* header, usize header_len,
                        u8 len_field_size, bool len_big_endian,
                        u8 checksum_size,
                        length_parser_cksum_type_t cksum_type,
                        length_parser_cksum_func_t cksum_func,
                        u32 cksum_init_val,
                        usize max_frame_len);

/**
 * @brief 尝试获取一个完整数据帧
 * @return LENGTH_PARSER_OK              成功
 * @return LENGTH_PARSER_NEED_MORE       数据不足
 * @return LENGTH_PARSER_ERR_INVALID_LEN 长度非法
 * @return LENGTH_PARSER_ERR_CHECKSUM    校验失败
 * @return LENGTH_PARSER_ERR_SYNC        帧头不匹配
 */
length_parser_result_t length_parser_get_frame(length_parser_t* self, usize* out_len);

/**
 * @brief 消费当前帧
 */
void length_parser_consume(length_parser_t* self);

/**
 * @brief 重置解析器状态
 */
void length_parser_reset(length_parser_t* self);
```

### 校验函数类型

```c
typedef enum {
    LENGTH_PARSER_CKSUM_NONE = 0,   /* 无校验 */
    LENGTH_PARSER_CKSUM_U8,         /* 8位校验和 */
    LENGTH_PARSER_CKSUM_CRC16,      /* CRC-16 */
    LENGTH_PARSER_CKSUM_CRC32,      /* CRC-32 */
} length_parser_cksum_type_t;

typedef union {
    void* null_fn;
    u8 (*checksum_u8)(const u8* data, usize len, u8 prev);
    u16 (*crc16)(const void* data, usize len, u16 prev);
    u32 (*crc32)(const void* data, usize len, u32 prev);
} length_parser_cksum_func_t;
```

### 使用示例

#### 无校验简单协议

```c
/* 帧格式: [u16 len LE][data...] */
length_parser_t parser;
length_parser_cksum_func_t cksum = { .null_fn = NULL };
length_parser_init(&parser, &ds, NULL, 0, 2, false, 0,
                   LENGTH_PARSER_CKSUM_NONE, cksum, 0, 1024);

usize data_len;
if (length_parser_get_frame(&parser, &data_len) == LENGTH_PARSER_OK) {
    /* data_len 是数据部分长度，不含长度字段 */
    u8 data[1024];
    dstream_peek(&ds, 2, data, data_len);  /* 跳过 2 字节长度字段 */
    length_parser_consume(&parser);
}
```

#### 带帧头和校验

```c
/* 帧格式: [0x55 0xAA][u8 len][data...][u8 checksum] */
static u8 simple_sum(const u8* data, usize len, u8 prev) {
    u8 sum = prev;
    for (usize i = 0; i < len; i++) sum += data[i];
    return sum;
}

length_parser_t parser;
u8 header[] = {0x55, 0xAA};
length_parser_cksum_func_t cksum = { .checksum_u8 = simple_sum };
length_parser_init(&parser, &ds, header, 2, 1, false, 1,
                   LENGTH_PARSER_CKSUM_U8, cksum, 0, 256);

usize data_len;
length_parser_result_t ret = length_parser_get_frame(&parser, &data_len);
if (ret == LENGTH_PARSER_OK) {
    u8 data[256];
    dstream_peek(&ds, 3, data, data_len);  /* 跳过 header(2) + len(1) */
    length_parser_consume(&parser);
} else if (ret == LENGTH_PARSER_ERR_CHECKSUM) {
    /* 校验失败，解析器已自动跳过 1 字节并重置 */
}
```

#### 大端序 + CRC-16

```c
/* 帧格式: [u16 len BE][data...][u16 crc] */
extern u16 crc16_ccitt(const void* data, usize len, u16 prev);

length_parser_t parser;
length_parser_cksum_func_t cksum = { .crc16 = crc16_ccitt };
length_parser_init(&parser, &ds, NULL, 0, 2, true, 2,  /* 大端 */
                   LENGTH_PARSER_CKSUM_CRC16, cksum, 0xFFFF, 512);
```

---

## 常见用例

### 用例 1：串口 DMA + 环形缓冲区

```c
/* 使用 DMA 接收，环形缓冲区存储 */
ring_buffer_t dma_rx_buffer;
dstream_t ds = { .buf_obj = &dma_rx_buffer, .ops = &ring_ops };

delimiter_parser_t parser;
delimiter_parser_init(&parser, &ds, NULL, 0, (u8*)"\r\n", 2, 256);

/* DMA 接收完成回调 */
void DMA1_Channel5_IRQHandler(void) {
    usize received = DMA_GetCurrDataCounter(DMA1_Channel5);
    dma_rx_buffer.head += received;  /* 更新写指针 */
}

/* 主循环处理 */
void main_loop(void) {
    usize len;
    while (delimiter_parser_get_frame(&parser, &len) == DELIMITER_PARSER_OK) {
        process_frame(&ds, len);
        delimiter_parser_consume(&parser);
    }
}
```

### 用例 2：协议自动检测

```c
/* 根据 first byte 判断协议类型 */
u8 first = dstream_peek_u8(&ds, 0);

if (first == 0x55) {
    /* 长度前置协议 */
    length_parser_get_frame(&len_parser, &len);
} else {
    /* 定界符协议 */
    delimiter_parser_get_frame(&delim_parser, &len);
}
```

### 用例 3：错误恢复

```c
length_parser_result_t ret = length_parser_get_frame(&parser, &len);

switch (ret) {
case LENGTH_PARSER_OK:
    /* 正常处理 */
    length_parser_consume(&parser);
    break;
case LENGTH_PARSER_ERR_SYNC:
    log_debug("Frame sync lost, retrying...\n");
    /* 解析器已自动跳过 1 字节，继续等待 */
    break;
case LENGTH_PARSER_ERR_CHECKSUM:
    log_debug("Checksum error, retrying...\n");
    /* 解析器已自动跳过 1 字节，继续等待 */
    break;
case LENGTH_PARSER_ERR_INVALID_LEN:
    log_debug("Invalid length field\n");
    break;
case LENGTH_PARSER_NEED_MORE:
    /* 等待更多数据 */
    break;
}
```

---

## 常见问题

**Q: 定界符解析器没有尾部时如何判断帧结束？**

A: 这种情况需要配合超时机制。例如设置一个定时器，在数据到达时重置，超时后认为帧结束。也可以在协议层面规定最大帧长度，通过 `max_frame_len` 参数处理。

**Q: 头部和尾部相同时如何工作？**

A: 解析器会在匹配到尾部后继续读取数据来确认。如果尾部后没有数据，需要等待更多数据到达。建议在帧尾后添加填充字节或使用更复杂的定界符。

**Q: 校验失败后如何恢复？**

A: 解析器会自动跳过 1 字节并重置状态机，尝试重新同步。调用者只需继续调用 `get_frame` 即可。

**Q: 如何支持 CRC-32 等复杂校验？**

A: 实现对应的增量 CRC 函数：
```c
u32 my_crc32(const void* data, usize len, u32 prev) {
    /* 增量计算，prev 是上一次的结果 */
    u32 crc = prev;
    const u8* p = (const u8*)data;
    for (usize i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}
```

**Q: 解析器是否线程安全？**

A: 解析器本身不使用全局变量，状态都在结构体中。如果在多线程环境使用，需要确保同一解析器不会被并发调用。

**Q: 最大帧长度应该设置多大？**

A: 取决于协议设计和可用内存。建议设置为实际可能出现的最大帧长 + 一定余量，过大会浪费内存，过小会导致正常帧被误判为超长。

---

## 编译运行

```bash
# 编译项目
xmake build

# 运行定界符解析器测试
xmake run test-delimiter_parser

# 运行长度前置解析器测试
xmake run test-length_parser
```

---

## 性能指标

| 指标 | 定界符解析器 | 长度前置解析器 |
|------|-------------|---------------|
| 内存占用 | ~48 字节 | ~56 字节 |
| 状态机复杂度 | O(n) | O(n) |
| 回溯支持 | 是 | 否 |
| 校验支持 | 无 | U8/CRC16/CRC32 |
| 错误恢复 | 滑动窗口 | 跳过重同步 |

---

## 文件列表

| 文件 | 说明 |
|------|------|
| `dstream.h` | 数据流核心接口 |
| `delimiter_parser.h/c` | 定界符解析器 |
| `length_parser.h/c` | 长度前置解析器 |
| `ring_buffer.h/c` | 环形缓冲区实现 |
| `fixed_buffer.h/c` | 固定缓冲区实现 |
| `pingpong_buffer.h/c` | 乒乓缓冲区实现 |
