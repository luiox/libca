## 为什么要有em_dstream？

em_dstream的目的是实现一套统一的数据缓冲和解析中间件，对于数据缓冲，提供各种缓冲区，对于解析，提供统一的接口，并且提供一些标准化的解析器工具。

最终，在嵌入式中，数据接收和帧截断这个事情我称作为前端部分，而后端是一个数据帧内的协议解析。中间我们通过一个dstream_t对象传递，前端持有dstream_t，可能利用dstream_t的操作往里面写入数据，也可能针对不同情况下直接利用一些特殊手段直接操作底层缓冲区，比如利用一次性的DMA、循环的DMA等写入，但是上层协议解析无需关心具体底层是连续的内存还是循环的缓冲区还是分散内存组成的缓冲区。

## 数据帧解析器

### 定界符解析器

定界符解析器（`delimiter_parser`）用于处理以特定字节序列作为帧边界的数据协议，典型场景包括：
- AT 指令（`\r\n` 结尾）
- HDLC 帧（`0x7E` 头尾）
- 自定义协议（特定头部/尾部标识）

**核心设计：**
- 状态机驱动，支持流式解析（数据可分批到达）
- 零拷贝设计：解析过程仅通过 `peek` 查看数据，不移动 cursor
- 支持头部、尾部或两者同时存在的定界方式
- 自动处理尾部匹配回溯（如 `\r\r\n` 正确匹配 `\r\n`）
- 超长帧检测与自动恢复（滑动窗口机制）

#### 状态转移图

```mermaid
stateDiagram-v2
    [*] --> IDLE: init

    IDLE --> IDLE: 头部不匹配\n(skip 1 byte)
    IDLE --> IN_FRAME: 头部匹配完成\n(或无头部)
    IDLE --> IDLE: 数据不足\n(返回 NEED_MORE)

    IN_FRAME --> IN_FRAME: 字节不匹配尾部
    IN_FRAME --> TRAILER_MATCH: 匹配尾部首字节
    IN_FRAME --> FRAME_READY: 单字节尾部匹配
    IN_FRAME --> IDLE: 帧超长\n(skip 1, 返回 ERR_FRAME_TOO_LONG)
    IN_FRAME --> IDLE: 数据不足\n(返回 NEED_MORE)

    TRAILER_MATCH --> FRAME_READY: 尾部完整匹配\n(返回 OK)
    TRAILER_MATCH --> TRAILER_MATCH: 继续匹配\n(部分匹配)
    TRAILER_MATCH --> IN_FRAME: 匹配失败\n(非尾部起始)
    TRAILER_MATCH --> TRAILER_MATCH: 回溯匹配\n(当前字节=尾部首字节)
    TRAILER_MATCH --> IDLE: 帧超长\n(skip 1, 返回 ERR_FRAME_TOO_LONG)
    TRAILER_MATCH --> IDLE: 数据不足\n(返回 NEED_MORE)

    FRAME_READY --> IDLE: consume()
```

#### 解析示例：AT 指令（`\r\n` 结尾）

```mermaid
sequenceDiagram
    participant U as UART/DMA
    participant B as Buffer
    participant P as Parser
    participant A as Application

    Note over U,A: 数据流: "AT+RST\r\n"

    rect rgb(240, 248, 255)
        Note over U,A: 阶段1: 部分数据到达
        U->>B: 写入 "AT+RS"
        A->>P: get_frame()
        P->>B: peek 数据
        P-->>A: NEED_MORE (无完整帧)
    end

    rect rgb(255, 250, 240)
        Note over U,A: 阶段2: 帧完成
        U->>B: 写入 "T\r\n"
        A->>P: get_frame()
        P->>B: peek "AT+RST\r\n"
        Note right of P: IDLE → IN_FRAME → TRAILER_MATCH<br/>匹配 \r\n 成功
        P-->>A: OK, len=8
        A->>B: peek(0, buf, 8) 读取帧数据
        A->>P: consume()
        P->>B: skip(8)
        Note right of B: cursor 移动，帧被消费
    end
```

#### 尾部匹配回溯示例

当尾部为 `\r\n`，收到 `\r\r\n` 时的处理：

```mermaid
sequenceDiagram
    participant B as Buffer
    participant P as Parser
    participant S as State

    Note over B,S: 输入: "DATA\r\r\n", 尾部="\r\n"

    P->>B: peek byte 'D'
    P->>S: IN_FRAME (不匹配 \r)
    P->>B: peek byte 'A'
    P->>S: IN_FRAME
    P->>B: peek byte 'T'
    P->>S: IN_FRAME
    P->>B: peek byte 'A'
    P->>S: IN_FRAME
    
    P->>B: peek byte '\r'
    Note right of P: 匹配尾部首字节
    P->>S: TRAILER_MATCH (match_len=1)
    
    P->>B: peek byte '\r'
    Note right of P: 期望 '\n'，收到 '\r'<br/>回溯: '\r' 可作为新起始
    P->>S: TRAILER_MATCH (match_len=1)
    
    P->>B: peek byte '\n'
    Note right of P: 匹配尾部第二字节
    P->>S: FRAME_READY (match_len=2)
```

**使用限制：**
- 只有头部没有尾部时，帧无法自动结束，需配合超时机制
- 头部和尾部相同时，需在尾部后有数据才能触发帧检测

---

### 长度前置解析器

长度前置解析器（`length_parser`）用于处理 `[Header][Len][Data...][Checksum]` 格式的数据帧，典型场景包括：
- Modbus RTU
- 自定义串口协议
- 网络数据包（带长度字段）

**核心设计：**
- 支持可选帧头（如 `\x55\xAA`）
- 长度字段支持 1/2/4 字节，大小端可配置
- 校验支持：无校验、8位校验和、CRC-16、CRC-32
- 联合体函数签名，支持不同类型的增量校验计算
- 错误自动恢复：校验失败或长度非法时自动跳过 1 字节重新同步

#### 状态转移图

```mermaid
stateDiagram-v2
    [*] --> IDLE: init

    IDLE --> IDLE: 帧头不匹配\n(skip 1, 返回 ERR_SYNC)
    IDLE --> IDLE: 数据不足\n(返回 NEED_MORE)
    IDLE --> LEN_PARTIAL: 帧头匹配\n开始读取Len
    IDLE --> DATA_AND_CKSUM: Len字段完整\n(足够数据时)
    IDLE --> IDLE: Len非法\n(skip 1, 返回 ERR_INVALID_LEN)

    LEN_PARTIAL --> LEN_PARTIAL: 继续读取
    LEN_PARTIAL --> DATA_AND_CKSUM: Len读取完成
    LEN_PARTIAL --> IDLE: 数据不足\n(返回 NEED_MORE)

    DATA_AND_CKSUM --> IDLE: 数据完整\n返回 OK
    DATA_AND_CKSUM --> IDLE: 校验失败\n(skip 1, 返回 ERR_CHECKSUM)
    DATA_AND_CKSUM --> IDLE: 数据不足\n(返回 NEED_MORE)
```

#### 解析示例：带帧头和校验的协议

帧格式：`[0x55 0xAA][u8 len][data...][u8 checksum]`

```mermaid
sequenceDiagram
    participant U as UART/DMA
    participant B as Buffer
    participant P as Parser
    participant A as Application

    Note over U,A: 完整帧: 55 AA 04 01 02 03 04 0A

    rect rgb(240, 248, 255)
        Note over U,A: 阶段1: 帧头到达
        U->>B: 写入 55 AA
        A->>P: get_frame()
        P->>B: peek 帧头
        Note right of P: 验证 header [55 AA]
        P-->>A: NEED_MORE (等待Len字段)
    end

    rect rgb(255, 250, 240)
        Note over U,A: 阶段2: Len字段到达
        U->>B: 写入 04
        A->>P: get_frame()
        P->>B: peek Len = 0x04
        Note right of P: target_len = 4
        P-->>A: NEED_MORE (等待数据)
    end

    rect rgb(240, 255, 240)
        Note over U,A: 阶段3: 数据和校验到达
        U->>B: 写入 01 02 03 04 0A
        A->>P: get_frame()
        P->>B: peek data [01 02 03 04]
        P->>P: calc_checksum(01+02+03+04) = 0x0A
        P->>B: peek checksum = 0x0A
        Note right of P: 校验通过 ✓
        P-->>A: OK, data_len=4
        A->>P: consume()
        Note right of B: skip(header+len+data+cksum)<br/>= skip(2+1+4+1)
    end
```

#### 错误恢复示例：校验失败

```mermaid
sequenceDiagram
    participant U as UART
    participant B as Buffer
    participant P as Parser
    participant A as Application

    Note over U,A: 错误帧: 55 AA 04 01 02 03 04 FF (校验错误)
    Note over U,A: 后续正确帧: 55 AA 02 AB CD 78

    U->>B: 写入完整数据
    A->>P: get_frame()
    P->>B: peek header [55 AA] ✓
    P->>B: peek len = 0x04 ✓
    P->>B: peek data + calc checksum
    Note right of P: 计算值 = 0x0A<br/>期望值 = 0xFF
    P-->>A: ERR_CHECKSUM
    Note right of P: 自动 skip(1), 重置状态

    A->>P: get_frame() (重试)
    Note right of P: cursor已在 55 AA 之后<br/>从 AA 开始匹配
    P->>B: peek [AA 04 01...] 
    Note right of P: header不匹配<br/>继续skip(1)
    P-->>A: ERR_SYNC

    A->>P: get_frame() (重试)
    Note right of P: cursor继续滑动<br/>直到找到新的帧头
    
    Note over U,A: ...经过多次重试...
    
    Note right of P: 最终匹配到正确帧
    P-->>A: OK, data_len=2
```

#### 校验函数设计

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

**配置参数：**
| 参数 | 说明 |
|------|------|
| `len_field_size` | 长度字段字节数 (1/2/4) |
| `len_big_endian` | 长度字段字节序 |
| `checksum_size` | 校验字段字节数 (0/1/2/4) |
| `checksum_big_endian` | 校验字段字节序 |
| `cksum_init_val` | 校验初始值（如 CRC-16 用 0xFFFF）|

**错误处理：**
- `ERR_SYNC`：帧头不匹配，自动跳过 1 字节
- `ERR_INVALID_LEN`：长度字段值超出 `max_frame_len`
- `ERR_CHECKSUM`：校验失败，自动跳过 1 字节


