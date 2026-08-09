# libca_log

日志门面与可插拔后端。命名空间 `ca::log`，构建目标 `libca_log`。

重新设计（#189）后：按 target 分发、门面与后端 fmt 解耦、零依赖 SimpleLogBackend、
spdlog 可选。详细设计见 `doc/log设计文档.md`。

## 快速上手

```cpp
#include "libca/log/log_macros.hpp"
#include "libca/log/simple_log_backend.hpp"
#include "libca/log/logger.hpp"
#include "libca/log/logger_registry.hpp"

using namespace ca::log;

// 1. 注册 default target 的 logger，绑定零依赖后端。
SimpleLogConfig cfg;
cfg.stream      = SimpleLogConfig::Stream::Stderr;
cfg.color       = true;
cfg.show_location = true;
LoggerRegistry::register_logger("default",
                                std::make_shared<Logger>(
                                    std::make_shared<SimpleLogBackend>(cfg)));

// 2. 打日志（用 default target）。
CA_LOG_INFO("server started on port {}", 8080);
CA_LOG_WARN("slow query: {} ms", 120);

// 3. 多模块各自独立级别：net 模块用 Error 级别。
auto net_logger = std::make_shared<Logger>(std::make_shared<SimpleLogBackend>());
net_logger->set_level(Level::Error);
LoggerRegistry::register_logger("net", net_logger);
CA_LOGT_ERROR("net", "connection refused");   // 输出
CA_LOGT_DEBUG("net", "handshake ok");         // 被 net 的 Error 级别过滤
```

## 宏 API

| 宏 | target | 说明 |
|----|--------|------|
| `CA_LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL(fmt, ...)` | `"default"` | 常用入口 |
| `CA_LOGT_<LEVEL>(target, fmt, ...)` | 指定 | 按模块分发 |

格式串用 fmt 风格：`CA_LOG_INFO("user {} logged in", id)`。

未注册的 target 静默丢弃；编译期级别 `CA_COMPILE_LOG_LEVEL`（默认 Info）以下的宏在编译期裁剪，
零运行期开销。

## 后端

### SimpleLogBackend（默认，零依赖）

```cpp
SimpleLogConfig cfg;
cfg.stream        = SimpleLogConfig::Stream::Stderr;  // 或 Stdout
cfg.file_path     = "app.log";   // 非空则同时写文件（追加）
cfg.color         = true;        // 终端彩色（仅 TTY）
cfg.show_location = true;        // 输出 file:line
cfg.show_target   = true;        // 输出 [target]
cfg.time_format   = "%H:%M:%S";  // 留空则不输出时间
SimpleLogBackend backend(cfg);
```

同步直写（调用线程在 mutex 下 fwrite），线程安全。格式：
`[time] [LEVEL] [target] message (file:line)`。

### SpdlogBackend（可选，with_spdlog=y）

```cpp
auto spd_logger = spdlog::stdout_color_mt("console");
SpdlogBackend backend(spd_logger);   // 适配新 ILogBackend
```

构建时 `xmake f --with_spdlog=y` 启用。pattern/level/sink 由调用方在 spdlog logger 上设定。

## 构建选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `--with_spdlog=y` | off | 编译 SpdlogBackend，拉取 spdlog 包 |

默认构建（不开 with_spdlog）只产出 SimpleLogBackend，零外部重依赖（除门面必需的 fmt）。

## 设计要点

- **按 target 分发**：`LoggerRegistry::get(target)`，每个 target 独立 logger/级别。根治了旧
  设计单一全局 logger 的并发竞态。
- **门面-后端解耦**：后端只 include `logger.hpp`，不依赖 fmt。格式化参数经 `OpaqueFormat`
  类型擦除传递，后端调 `render_to(buf)` 取字符串。
- **级别管理收归 Logger**：后端只输出，不再各写一套 set_level。
- **编译期 + 运行期双重过滤**：低级别日志可在 Release 构建里编译期裁剪。

详见 `doc/log设计文档.md`。
