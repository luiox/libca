# em_log 轻量级日志组件设计

## 1. 设计目标
*   **低开销**：最小化对 CPU 和内存的占用。
*   **异步化**：支持将日志先存入缓冲区，再由低优先级任务写入 Flash 或串口。
*   **多后端**：支持 UART、RTT、Flash 文件系统 (FatFs) 等。
*   **线程安全**：在 FreeRTOS 环境下支持多任务并发打印。

## 2. 核心架构

### 2.1 日志分级 (Log Levels)
```c
typedef enum {
    EM_LOG_NONE = 0,
    EM_LOG_ERROR,
    EM_LOG_WARN,
    EM_LOG_INFO,
    EM_LOG_DEBUG,
    EM_LOG_VERBOSE
} em_log_level_t;
```

### 2.2 后端抽象 (Backend Interface)
```c
typedef struct {
    const char* name;
    void (*init)(void);
    void (*output)(const char* str, uint32_t len);
    void (*flush)(void);
} em_log_backend_t;
```

### 2.3 异步缓冲区
利用 `em_base/ringbuffer` 实现。
- `log_printf` -> 格式化字符串 -> 写入 RingBuffer。
- `log_task` (低优先级) -> 从 RingBuffer 读取 -> 调用 `backend->output`。

## 3. 关键功能实现

### 3.1 格式化输出
支持类似 `[Time][Level][Tag] Message` 的格式。
时间戳可以通过 `HAL_GetTick()` 或 RTC 获取。

### 3.2 静态过滤与动态过滤
- **静态过滤**：通过宏定义 `EM_LOG_LEVEL_DEFAULT` 在编译时剔除低级别日志，节省 Flash 空间。
- **动态过滤**：运行时通过 `em_log_set_level(level)` 调整输出。

### 3.3 异常现场保存 (Panic Log)
当系统发生 `HardFault` 或断言失败时：
1.  停止所有中断。
2.  直接同步调用 Flash 后端，将关键寄存器和堆栈信息写入 Flash 的特定扇区。
3.  下次启动时，`em_ota` 或 `em_log` 可以读取该区域并上报给上位机。

## 4. 与 EasyLogger 的区别
- **更轻量**：剔除不必要的复杂配置。
- **深度集成**：直接使用 `libca` 的 `ringbuffer` 和 `string_util`。
- **针对性优化**：专门为 OTA 过程中的错误记录优化存储逻辑。
