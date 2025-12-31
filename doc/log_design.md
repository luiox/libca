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
typedef struct em_log_backend {
    const char* name;
    em_log_level_t min_level; // 后端独立过滤：该后端接收的最低日志级别
    bool enabled;             // 是否启用该后端
    bool support_color;       // 是否支持彩色输出 (ANSI)
    void (*init)(struct em_log_backend* backend);
    void (*output)(struct em_log_backend* backend, const char* str, usize len);
    void (*flush)(struct em_log_backend* backend);
    struct em_log_backend* next; // 链表指针，支持多个后端
} em_log_backend_t;
```
- **多后端绑定**：系统维护一个后端链表。通过 `em_log_backend_register(backend)` 将不同的输出器（如 UART, SD Card, RTT）挂载到系统。
- **独立过滤**：每条日志在分发时，会遍历链表，并根据每个后端的 `min_level` 决定是否输出。这样可以实现“控制台输出 INFO 以上，文件记录 DEBUG 以上”的差异化配置。

### 2.3 异步驱动机制 (Asynchronous Mechanism)
`em_log` 的异步化深度依赖 `em_base/async` 组件提供的轻量级工作队列：
- **生产者 (Producer)**：`log_printf` 将格式化后的字符串推入日志专用的 `RingBuffer`，随后调用 `em_async_submit` 提交一个“日志分发任务”到全局异步队列。
- **中转站 (Work Queue)**：`em_async` 维护一个任务队列。在提交任务时，可以通过 `on_notify` 回调唤醒 RTOS 任务或设置标志位。
- **消费者 (Consumer)**：
    - **RTOS 环境**：在一个独立的低优先级任务中循环调用 `em_async_process(async, 0)`。
    - **裸机环境**：在 `main` 循环中调用 `em_async_process(async, max_items)`。
- **平衡策略 (Balancing)**：在单线程（裸机）环境下，通过 `max_items` 限制单次轮询处理的任务数，防止日志分发占用过多 CPU 时间，从而平衡日志输出与主控制流。
- **解耦输出**：`em_async` 只负责触发“分发任务”。具体的后端输出（如 UART DMA）由后端自行实现，`em_log` 仅负责将数据从日志缓冲区传递给后端接口。

### 2.4 溢出策略 (Overflow Strategy)
利用 `em_base/ringbuffer` 实现。
- **异步流程**：`log_printf` -> 格式化字符串 (不含颜色) -> 写入日志 RingBuffer -> 提交 `em_async` 任务 -> `em_async_process` 执行任务 -> 遍历所有已注册且启用的 `backend` -> 根据 `min_level` 过滤 -> (可选) 后端着色 -> 调用 `backend->output`。
- **策略选择**：支持通过宏或配置选择：
    - **Overwrite (覆盖)**：缓冲区满时覆盖最旧数据，适用于“黑匣子”模式。
    - **Discard (丢弃)**：缓冲区满时丢弃新日志，保证现有日志完整性。
    - **Block (阻塞)**：仅在非中断环境下可选，确保日志不丢失。

### 2.5 中断安全 (ISR Safety)
- **命名约定**：提供 `_isr` 后缀的 API（如 `em_log_printf_isr`）。
- **实现机制**：
    - **原子性**：RingBuffer 写入必须是原子操作或受中断锁保护。
    - **非阻塞**：ISR 接口严禁使用信号量或延时函数。
    - **自动降级**：若在中断中误调同步接口，系统应能识别并自动转为异步非阻塞模式或安全丢弃。

## 3. 关键功能实现

### 3.1 格式化输出与时间戳
- **格式**：支持 `[Time][Level][Tag] Message`。
- **时间戳抽象**：提供回调接口 `em_log_get_time_func_t`，允许用户自定义时间来源（如 RTOS Tick, CPU Cycles, RTC）。默认使用 `soft_timer` 提供的毫秒级计数。

### 3.2 多级过滤与优化
- **静态过滤 (Zero Cost)**：通过宏 `EM_LOG_LEVEL_DEFAULT` 在编译期剔除。利用 `__builtin_expect` 优化分支预测，确保关闭的日志级别对性能几乎零影响。
- **动态过滤**：
    - **全局级别**：`em_log_set_level(level)`，影响所有后端。
    - **标签过滤 (Tag-based)**：支持为特定模块（如 `NETWORK`）设置独立的日志级别。
- **后端过滤**：每个后端（UART/Flash）可拥有独立的过滤级别 `min_level`。只有当日志级别高于或等于 `max(global_level, backend_min_level)` 时，该后端才会输出。

### 3.3 异常现场保存 (Panic Log)
当系统发生 `HardFault` 或断言失败时：
1.  停止所有中断。
2.  **自动 Flush**：强制将当前 RingBuffer 中的剩余日志同步写入后端。
3.  直接同步调用 Flash 后端，将关键寄存器和堆栈信息写入 Flash 的特定扇区。
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
- **格式化缓冲区**：`log_printf` 内部使用互斥锁保护一个静态的格式化缓冲区，或者在栈空间足够的情况下使用局部变量，以确保多任务并发时的线程安全。
- **标签过滤优化**：标签 (Tag) 建议使用结构体指针比较而非字符串比较。
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
