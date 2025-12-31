# em_log 轻量级日志组件设计

## 1. 设计目标
*   **低开销**：最小化对 CPU 和内存的占用。
*   **异步化**：支持将日志先存入缓冲区，再由低优先级任务写入 Flash 或串口。也应该提供同步的选项
*   **多后端**：支持 UART、RTT、Flash 文件系统 (FatFs) 等。
*   **线程安全**：在 FreeRTOS 环境下支持多任务并发打印。
*   **可控的彩色输出**: 通过宏配置是否使用ANSI转移的彩色输出
*   **可控的格式化输出**: 时间戳应该以`[程序启动以后的秒数:毫秒数:微秒数]`这样子为默认情况（这个通过提供一个由soft_timer提供的接口实现的函数来解决），格式化应该是可以可以外部配置的
*   **高性能的文件与行号信息处理**: 尽可能在编译期间拼接行号和文件信息，利用宏，尤其是trace和debug，一般来说需要带文件和行号

## 2. 核心架构

### 2.1 日志分级 (Log Levels)
```c
typedef enum em_log_level{
    EM_LOG_NONE = 0,
    EM_LOG_ERROR,
    EM_LOG_WARN,
    EM_LOG_INFO,
    EM_LOG_DEBUG,
    EM_LOG_VERBOSE
} em_log_level_t;
```

### 2.2 后端抽象与多后端支持 (Backend Interface & Multi-backend)
```c
typedef struct em_log_record {
    em_log_level_t level;
    uint32_t timestamp;
    const char* tag;      // 指向 Tag 字符串（若使用 Hash 则为 NULL 或 ID）
    const char* payload;  // 格式化好的日志内容
    size_t payload_len;
} em_log_record_t;

typedef struct em_log_backend {
    const char* name;
    em_log_level_t min_level; // 后端独立过滤：该后端接收的最低日志级别
    bool enabled;             // 是否启用该后端
    bool support_color;       // 是否支持彩色输出 (ANSI)
    void (*init)(struct em_log_backend* backend);
    void (*output)(struct em_log_backend* backend, const em_log_record_t* record);
    void (*panic_output)(struct em_log_backend* backend, const em_log_record_t* record); // 紧急模式下的同步输出（轮询模式，禁止中断依赖）
    void (*flush)(struct em_log_backend* backend);
    struct em_log_backend* next; // 链表指针，支持多个后端
} em_log_backend_t;
```
- **多后端绑定**：系统维护一个后端链表。通过 `em_log_backend_register(backend)` 将不同的输出器（如 UART, SD Card, RTT）挂载到系统。
- **独立过滤**：每条日志在分发时，会遍历链表，并根据每个后端的 `min_level` 决定是否输出。
- **灵活输出**：`output` 接口接收结构化的 `em_log_record_t`。
    - **UART 后端**：可以现场拼接时间戳和颜色代码：`printf("[%d] %s", record->timestamp, record->payload)`。
    - **Flash 后端**：可以直接存储二进制 Header 和 Payload，节省空间并减少字符串转换开销。
- **Panic 支持**：`panic_output` 必须实现为死循环轮询发送（Polling Mode），严禁依赖中断或操作系统服务，确保在 HardFault 或关中断场景下仍能输出。

### 2.3 异步驱动机制 (Asynchronous Mechanism)
`em_log` 的异步化深度依赖 `em_base/async` 组件，但为了避免高频日志淹没任务队列，采用 **"边缘触发 (Edge Triggered)"** 机制：
- **生产者 (Producer)**：`log_printf` 将日志包写入 RingBuffer。仅当检测到“日志处理任务未处于活动状态”时，才调用 `em_async_submit` 提交任务。
- **消费者 (Consumer)**：
    - 任务被唤醒后，循环从 RingBuffer 读取数据包并分发，直到 RingBuffer 为空。
    - 处理完毕后，清除“活动状态”标志，等待下一次唤醒。
- **优势**：无论日志频率多高，在同一时刻 `em_async` 队列中最多只有一个日志分发任务，极大降低了调度开销。

### 2.4 数据协议与溢出策略 (Data Protocol & Overflow Strategy)
为了支持高效过滤和元数据保留，RingBuffer 内部存储**结构化数据包 (TLV)** 而非纯文本：
- **Packet 结构**：`[Header: TotalLen | Level | TagHash | Timestamp] [Payload: Formatted String]`
    - **Header**：定长头部，包含日志级别、Tag 哈希值（用于快速过滤）、**生产者时间戳**。
    - **Payload**：格式化后的日志内容（不含颜色）。
- **处理流程**：
    1.  **Producer**: 获取时间戳 -> 生成 Header -> 格式化内容 -> 原子写入 RingBuffer。
    2.  **Consumer**: 读取 Header -> 检查 `backend->min_level` -> (若通过) 读取 Payload 并输出 -> (若不通过) 跳过 Payload。
- **溢出策略**：
    - **Discard (默认)**：RingBuffer 满时丢弃新日志，并统计丢弃计数。
    - **Overwrite**：覆盖最旧数据（需 RingBuffer 支持覆盖写模式）。
    - **Block**：仅在非中断且允许阻塞的上下文中可选。

### 2.5 中断安全 (ISR Safety)
- **命名约定**：提供 `_isr` 后缀的 API（如 `em_log_printf_isr`）。
- **实现机制**：
    - **原子性**：RingBuffer 写入必须是原子操作或受中断锁保护。
    - **非阻塞**：ISR 接口严禁使用信号量或延时函数。
    - **自动降级**：若在中断中误调同步接口，系统应能识别并自动转为异步非阻塞模式或安全丢弃。

## 3. 关键功能实现

### 3.1 格式化输出与时间戳
- **格式**：支持 `[Time][Level][Tag] Message`。
- **时间戳捕获**：时间戳必须在 **生产者 (log_printf)** 阶段获取并存入 Packet Header，以确保日志时间的准确性，避免因异步队列堆积导致的时间滞后。
- **时间源**：提供回调接口 `em_log_get_time_func_t`，默认使用 `soft_timer`。

### 3.2 多级过滤与优化
- **静态过滤 (Zero Cost)**：通过宏 `EM_LOG_LEVEL_DEFAULT` 在编译期剔除。利用 `__builtin_expect` 优化分支预测，确保关闭的日志级别对性能几乎零影响。
- **动态过滤**：
    - **全局级别**：`em_log_set_level(level)`，影响所有后端。
    - **标签过滤 (Tag-based)**：支持为特定模块（如 `NETWORK`）设置独立的日志级别。
- **后端过滤**：每个后端（UART/Flash）可拥有独立的过滤级别 `min_level`。只有当日志级别高于或等于 `max(global_level, backend_min_level)` 时，该后端才会输出。

### 3.3 异常现场保存 (Panic Log)
当系统发生 `HardFault` 或断言失败时：
1.  停止所有中断 ( `__disable_irq()` )。
2.  **自动 Flush**：强制将当前 RingBuffer 中的剩余日志通过 `panic_output` 接口同步写入后端。
3.  直接同步调用 Flash 后端的 `panic_output`，将关键寄存器和堆栈信息写入 Flash 的特定扇区。
4.  下次启动时，`em_ota` 或 `em_log` 可以读取该区域并上报给上位机。

### 3.4 集成断言 (Integrated Assertions)
提供与日志系统深度绑定的断言宏，解决“防御性编程”与“性能开销”的矛盾：
- **`EM_ASSERT(expr)`**：逻辑断言，失败时触发 `Panic Log` 并 `flush` 缓冲区。
- **`EM_CHECK_PARAM(expr, ret)`**：专门用于函数入口参数检查（如 `NULL` 指针）。
    - **开发阶段**：开启 `EM_LOG_CONF_PARAM_CHECK_VERBOSE`，失败时会打印 `[ERROR] Invalid parameter in function XXX at line YYY`，帮助快速定位调用者。
    - **发布阶段**：关闭详细日志，仅保留 `if (!(expr)) return ret;`，确保系统鲁棒性且开销极小。
    - **极致性能**：通过宏完全剔除此类检查（不推荐，除非内存极其紧张）。

### 3.5 Hex Dump 工具
提供 `em_log_hexdump(tag, level, data, len)`，以标准 16 进制格式输出数据包，支持地址偏移和 ASCII 预览。

### 3.6 日志限流 (Throttling)
针对高频触发的日志（如传感器异常），支持限流机制：`EM_LOG_THROTTLE(ms, tag, level, fmt, ...)`，确保同一日志在指定时间内只打印一次。

### 3.7 线程安全与性能优化
- **零拷贝格式化 (Zero-Copy)**：
    - 优先尝试在 RingBuffer 中预留空间 (`reserve`)，直接将格式化结果写入 RingBuffer 内存，避免中间缓冲区的内存拷贝。
    - 若 RingBuffer 不支持预留写入，则回退到栈上分配临时缓冲区（如 `char buf[128]`），避免使用全局静态锁，以减少多任务竞争。
- **标签过滤优化**：标签 (Tag) 建议使用结构体指针比较或哈希值比较，而非字符串比较。
- **内存所有权**：`em_log_backend_t` 实例必须由调用者保证其生命周期（通常定义为 `static` 或全局变量），因为系统链表会长期持有其指针。

### 3.8 彩色输出策略
- **解耦存储与显示**：RingBuffer 中仅存储纯净的日志文本。
- **按需着色**：在分发阶段，如果 `backend->support_color` 为真，分发器会在调用 `output` 前后自动发送 ANSI 颜色转义码。这保证了串口看到的是彩色日志，而 Flash 文件中保存的是易于解析的纯文本。

### 3.9 多平台模拟与验证 (PC Simulation)
为了在 Windows/Linux 上验证异步逻辑，建议采用以下模拟方案：
- **方案 A：专用工作线程**。模拟 RTOS 环境，使用 `std::thread` 或系统原生线程运行 `async_process`，通过信号量同步。
- **方案 B：主线程分时**。模拟裸机环境，在 `main` 循环中调用 `async_process` 并设置 `max_items`，观察日志输出对主逻辑循环频率的影响。

## 4. 与 EasyLogger 的区别
- **更轻量**：剔除不必要的复杂配置。
- **深度集成**：直接使用 `libca` 的 `ringbuffer` 和 `string_util`。
- **针对性优化**：专门为 OTA 过程中的错误记录优化存储逻辑。
