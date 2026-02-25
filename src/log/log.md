# libca.log 使用文档

## 1. 模块简介

`libca.log` 是一个面向性能的 C++17 日志门面，核心目标：

- 编译期过滤：低于编译阈值的日志代码直接被编译器裁剪
- 运行时快速过滤：通过原子级别读取判断是否需要记录
- 惰性格式化：仅在通过过滤后才进行 `fmt` 参数格式化
- 后端解耦：通过 `ILogBackend` 适配不同日志实现（当前提供 `SpdlogBackend`）

---

## 2. 依赖与构建

`src/log/xmake.lua` 已配置：

- `fmt`（header-only）
- `spdlog`（header-only，且使用 external fmt）
- `doctest`（用于测试目标）

常用命令：

```bash
# 构建日志库
xmake -b -v libca.log

# 构建并运行日志测试
xmake -b -v test-libca.log
xmake run test-libca.log
```

---

## 3. 快速上手

### 3.1 创建并安装全局 logger

```cpp
#include "logger.hpp"
#include "spdlog_backend.hpp"

#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>

int main() {
    auto spd = spdlog::stdout_color_mt("app");
    auto backend = std::make_shared<libca::SpdlogBackend>(spd);

    // 安装为全局 logger
    libca::set_global_logger(backend);

    // 可选：设置运行时级别
    if (auto logger = libca::get_global_logger()) {
        logger->set_level(libca::Level::Info);
    }

    LOG_INFO("service started, pid={}", 1234);
    LOG_WARN("disk usage high: {}%", 87);
}
```

### 3.2 使用 target（业务模块）日志

```cpp
LOGT_INFO("network", "connect {}:{}", host, port);
LOGT_ERROR("db", "query failed, code={}", errCode);
```

---

## 4. 经典使用方式

### 4.1 分级日志

```cpp
LOG_TRACE("trace message");
LOG_DEBUG("debug message");
LOG_INFO("info message");
LOG_WARN("warn message");
LOG_ERROR("error message");
LOG_CRITICAL("critical message");
```

### 4.2 运行时调整日志级别

```cpp
if (auto logger = libca::get_global_logger()) {
    logger->set_level(libca::Level::Warn);
}
```

设置后：

- `Trace/Debug/Info` 会被运行时过滤
- `Warn/Error/Critical` 仍会输出

### 4.3 字符串与级别互转

```cpp
auto lvl = libca::stringToLevel("debug"); // => Level::Debug
auto s = libca::levelToString(libca::Level::Error); // => "Error"
```

---

## 5. API 接口参考

### 5.1 枚举与工具函数

- `enum class libca::Level`
  - `Trace, Debug, Info, Warn, Error, Critical, Off`
- `libca::stringToLevel(std::string_view)`
- `libca::levelToString(Level)`
- `libca::should_compile(Level)`

### 5.2 门面接口

#### `class libca::ILogBackend`

- `virtual void log(Level, fmt::string_view target, fmt::string_view file, int line, fmt::string_view formatStr, fmt::format_args args) = 0;`
- `virtual const std::atomic<Level>& get_level_atomic() const = 0;`
- `virtual void set_level(Level level) = 0;`

#### `class libca::Logger`

- `bool should_log(Level level) const;`
- `ILogBackend* backend() const;`
- `void set_level(Level level) const;`

#### 全局门面函数

- `void set_global_logger(std::shared_ptr<ILogBackend> backend);`
- `Logger* get_global_logger();`

### 5.3 日志宏

- 基础宏：`LOG_LEVEL(level, target, fmt, ...)`
- 无 target 快捷宏：`LOG_TRACE/LOG_DEBUG/LOG_INFO/LOG_WARN/LOG_ERROR/LOG_CRITICAL`
- 带 target 快捷宏：`LOGT_TRACE/LOGT_DEBUG/LOGT_INFO/LOGT_WARN/LOGT_ERROR/LOGT_CRITICAL`

说明：宏会自动注入 `__FILE__` 与 `__LINE__`。

---

## 6. 设计思路与原理

### 6.1 编译期过滤

通过：

- `COMPILE_LOG_LEVEL`（默认 2，即 `Info`）
- `kCompileTimeLevel`
- `if constexpr (should_compile(level))`

实现低级别日志在编译期直接裁剪，避免无意义代码进入最终二进制。

### 6.2 运行时过滤

`Logger` 在构造时缓存后端原子级别地址，`should_log` 仅执行：

- 原子读取（`memory_order_relaxed`）
- 常量时间级别比较

保证高频路径低开销。

### 6.3 惰性格式化

日志调用路径：

1. 编译期判断（`if constexpr`）
2. 运行时判断（`should_log`）
3. 通过后才调用 `fmt::make_format_args(...)` 并写入后端

从而避免关闭日志时的字符串格式化成本。

### 6.4 spdlog 适配层

`SpdlogBackend` 做了以下工作：

- `Level -> spdlog::level` 的查表映射（`constexpr std::array`）
- `thread_local spdlog::memory_buf_t` 复用缓冲，减少重复分配
- 通过 `spdlog::source_loc{file, line, ""}` 透传源码位置信息

### 6.5 全局 logger 生命周期与并发策略

当前实现为：

- 全局读取指针：`std::atomic<Logger*> g_logger_ptr`
- 生命周期持有：`std::shared_ptr<Logger> g_logger_holder`

优点：

- 读取路径无需 shared_ptr 引用计数操作
- 兼容 C++17，不依赖 C++20 的 `atomic<shared_ptr>` 特化

---

## 7. 常见问题

### Q1: 为什么日志不输出？

按顺序检查：

1. 是否调用了 `set_global_logger(...)`
2. 运行时级别是否过高（如 `Warn` 会过滤 `Info`）
3. 编译期级别是否裁剪了该日志（`COMPILE_LOG_LEVEL`）

### Q2: 如何修改编译期过滤阈值？

通过编译宏定义 `COMPILE_LOG_LEVEL`：

- `0=Trace`
- `1=Debug`
- `2=Info`
- `3=Warn`
- `4=Error`
- `5=Critical`
- `6=Off`

例如设置为 `3` 后，`Trace/Debug/Info` 会被编译期裁剪。

---

## 8. 测试覆盖

`test-libca.log` 目前覆盖：

- 级别字符串转换
- 运行时过滤逻辑（`Warn` 阈值下过滤 `Info`、放行 `Error`）

可在后续补充：

- 各宏的编译期行为验证
- 多线程并发日志下的行为验证
- `target/file/line` 元信息检查
