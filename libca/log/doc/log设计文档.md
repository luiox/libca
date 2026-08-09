---
version: 1.0
update:
2026-08-03 - 重新设计：LoggerRegistry 按 target 分发、门面-后端 fmt 解耦、可选 spdlog、零依赖 SimpleLogBackend
---

# libca_log 设计文档

## 1. 目标与边界

`libca_log` 为 C++17 项目提供日志门面与可插拔后端。重新设计（#189）解决旧实现的几类问题：

- **fmt 耦合泄漏到后端**：旧 `ILogBackend::log` 参数携带 `fmt::format_args`，后端必须依赖 fmt。
- **单一全局 logger 竞态**：旧 `set_global_logger` 先赋 holder 再 store 裸指针，存在窗口期。
- **级别管理重复**：旧设计把级别委派给后端，每加一个后端都要重写 set_level。
- **目录与命名不规范**：旧代码命名空间 `libca::`、xmake 在源码目录内、测试混在源码目录。
- **spdlog 硬依赖**：仅一个 spdlog 后端，零依赖场景无法使用。

新设计以 **LoggerRegistry 按 target 分发** 为核心，配合 **类型擦除的格式化参数载体**
（OpaqueFormat）与 **可选 spdlog**，从架构层面消除上述问题。

## 2. 分层

```
用户宏 (log_macros.hpp)
    │  CA_LOG_INFO("hello {}", name)
    ▼
detail::log_with_source(level, target, file, line, fmt_str, args...)
    │  1. 编译期裁剪 (CA_COMPILE_LOG_LEVEL)
    │  2. LoggerRegistry::get(target)  → 找不到则丢弃
    │  3. logger->should_log(level)    → 运行期过滤
    │  4. 构造 FmtArgsHolder (view)
    ▼
Logger::backend()->log(level, target, file, line, OpaqueFormat&)
    │
    ▼
ILogBackend 实现（后端只 include logger.hpp，零 fmt 依赖）
    ├─ SimpleLogBackend   (零依赖，同步直写，彩色 + 文件)
    └─ SpdlogBackend      (可选，with_spdlog=y)
```

门面层（`detail/fmt_format.hpp`、`log_macros.hpp`）依赖 fmt；后端层（`logger.hpp`、
`simple_log_backend.*`、`spdlog/`）只依赖 logger.hpp 与 level.hpp，**不 include fmt**。

## 3. OpaqueFormat —— 门面与后端的 fmt 解耦

`OpaqueFormat` 是一个抽象基类（纯虚 `render_to(std::string&)`）。门面侧的 `FmtArgsHolder`
是它的具体子类，持有 `fmt::string_view` + `fmt::format_args`（二者都是 view）。后端调
`message.render_to(buf)` 即可拿到格式化完成的字符串，完全不需要知道 fmt 的存在。

**关键技术约束**：`fmt::format_args` 是 view，指向调用方栈帧上的实参。因此
`FmtArgsHolder` 的生命周期 **不得超过单次 log 调用**：

- 同步调用 `backend->log()` 安全（实参在栈帧内存活）；
- **严禁跨线程入队**（实参已销毁，view 悬空）。异步需求必须由调用方在同步路径内先
  `render_to` 成 `std::string` 再入队。

这是选择方案 B（虚函数 render）而非"预格式化字符串"的根本理由：方案 B 让后端有机会按需
渲染（甚至不渲染，只看 level/target 做过滤），但代价是 view 生命周期约束。

> 为什么不传原始参数做结构化日志（如输出 JSON 字段）？因为 `fmt::format_args` 一旦构造就
> 擦除了参数名（只剩位置/类型），后端无法反推 `{name}` 对应的字段名。要结构化日志需另开
> KV 宏 API，不在本次范围。

## 4. LoggerRegistry —— 按 target 分发的竞态根治

旧设计的单一全局 `g_logger_ptr`（裸指针）+ `g_logger_holder`（shared_ptr）被并发 set/get，
存在"holder 已析构、ptr 仍指向"的窗口期。新设计用 **按 target 的注册表** 取代：

- 内部 `unordered_map<string, shared_ptr<Logger>>` + `shared_mutex`。
- `get(target)`：shared_lock，多读并发；返回 `Logger*`。
- `register_logger(target, logger)`：unique_lock，原子替换。
- `unregister_logger(target)`：unique_lock。

**竞态根治原理**：map 存的是 `shared_ptr<Logger>`。并发 register 替换某 target 时，旧 Logger
由 shared_ptr 引用计数保活——正在执行 `backend->log()` 的调用方持有完整栈帧，registry 的替换
不影响它；调用结束后引用计数归零才析构。不再有"裸指针被并发改"的问题。

`get()` 返回 `Logger*`（裸指针）是安全的，因为约定它只在单次 log 表达式内使用（栈帧存活期间
shared_ptr 引用计数 ≥ 1）。这与 spdlog::get、log4j 的 logger 句柄语义一致。

## 5. 级别管理收归 Logger

旧设计把 `set_level`/`get_level_atomic` 委派给后端，每个后端重复实现。新设计：

- `Logger` 持一个 `atomic<Level>`，`should_log`/`set_level`/`level` 在此完成。
- `ILogBackend` **不再有级别接口**，后端只负责输出已过滤的消息。

这样新增后端（如未来的 syslog/network backend）无需关心级别。

## 6. 编译期与运行期双重过滤

- **编译期**：`CA_COMPILE_LOG_LEVEL`（默认 Info=2）控制。低于该级别的日志在编译期被
  `if constexpr (should_compile(level))` 裁剪，零运行期开销。可通过
  `xmake` 的 `add_defines("CA_COMPILE_LOG_LEVEL=4")` 在 Release 构建里关闭 Trace/Debug。
- **运行期**：`Logger::should_log` 用 atomic 比较，每条日志一次原子读。

编译期裁剪已用 `static_assert` 验证（见 logger_test.cpp 顶部）。

## 7. 后端

### 7.1 SimpleLogBackend（默认，零依赖）

- 同步直写：调用线程在 mutex 保护下 fwrite，零外部依赖（不依赖 fmt/spdlog）。
- 配置（SimpleLogConfig）：stream（stdout/stderr）、file_path、color、show_location、
  show_target、time_format。
- 彩色：Windows 用 `SetConsoleTextAttribute`，POSIX 用 ANSI 转义；仅 TTY 时着色，避免重定向
  到文件时混入控制码。
- 文件用二进制追加（`ios::binary`），避免 Windows 文本模式 `\n`→`\r\n` 翻译。

### 7.2 SpdlogBackend（可选，with_spdlog=y）

- 适配底层 `spdlog::logger`，把 `OpaqueFormat::render_to` 的字符串喂给 spdlog（spdlog 不再做
  fmt 格式化，因消息已完整）。
- 保留 `source_loc`（file:line），Level 映射到 spdlog level。
- 仅 `with_spdlog=y` 时编译（根 xmake.lua 的 option + log/xmake.lua 的 has_config 分支）。

## 8. 依赖与构建

- 门面层（fmt_format.hpp / log_macros.hpp）：依赖 fmt（header_only）。这是 log 模块对外的
  唯一硬依赖——任何使用 `CA_LOG_*` 宏的代码都间接依赖 fmt。
- 后端层：SimpleLogBackend 零依赖；SpdlogBackend 依赖 spdlog（可选）。
- `with_spdlog` build option（默认 false）：控制是否编译 SpdlogBackend 与拉取 spdlog 包。
  作为 submodule 被引用时，默认不强制拉 spdlog，与 `with_tests`（默认 false 避免 gtest）策略一致。

## 9. 测试策略

- **level_test**：from_string/to_string 正反例、大小写不敏感。
- **logger_test**：should_log 级别过滤、set_level 原子性、宏到后端的完整路径（含 file:line/target
  元信息验证）、未注册丢弃、编译期裁剪（static_assert）。
- **registry_test**：注册/查找/注销、不同 target 独立级别、**多线程并发访问安全**（8 线程 ×
  500 ops 不崩溃）、**引用计数保活**（并发替换时 get 返回的指针仍可安全使用）。
- **simple_log_backend_test**：文件输出格式、各配置开关、多行追加、默认配置。
- **spdlog_backend_test**（with_spdlog）：消息路由、全级别映射、null 安全。

## 10. 与旧实现的不兼容变更

旧实现未接入构建、全库无调用方，故重新设计不保留任何向后兼容：

- 命名空间 `libca::` → `ca::log`。
- `stringToLevel`/`levelToString` → `from_string`/`to_string`（snake_case）。
- `set_global_logger`/`get_global_logger` → `LoggerRegistry::register_logger`/`get`。
- `LOG_INFO` → `CA_LOG_INFO`（加 CA_ 前缀避免污染用户命名空间）。
- `ILogBackend::log` 签名：去掉 `fmt::string_view`/`fmt::format_args`，改为 `OpaqueFormat&`。
- 后端 `get_level_atomic`/`set_level` 移除（级别管理收归 Logger）。
