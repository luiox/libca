---
version: 1.1
update:
2026-07-13 - 合并 dynamic_library 设计要点，删除逐接口 spec 引用，本文成为 core 唯一设计文档
2026-07-06 - 首版，补充 core 模块职责、错误模型、字节设施、RAII 辅助与平台边界
---

# libca::core 设计文档

> 本文只讲 core 模块的设计思路和边界。**具体 API 签名与逐方法说明见各头文件的 Doxygen 注释，本文不重复 API 清单。**
> 涉及的头文件：
> `datatype.hpp`、`result.hpp`、`status.hpp`、`bytes.hpp`、`scope_guard.hpp`、`cast.hpp`、`any.hpp`、
> `wrapper.hpp`、`dynamic_library.hpp`、`platform.hpp`、`stacktrace.hpp`、`math_util.hpp`、`array_util.hpp`、`dllexport.hpp`。

## 1. 模块定位

`libca::core` 是 libca C++ 部分的最低层基础模块，其它模块可以依赖 core，但 core 不依赖任何其它 libca 模块。

core 的职责是提供跨模块共享的基础语义：

- 固定宽度/语义类型：`datatype.hpp`
- 返回值错误模型：`Result<T, E>`、`Status`
- 字节序列与缓冲：`Bytes`、`BytesMut`、`ByteSlice`
- RAII 与作用域工具：`ScopeGuard`、`DEFER`
- 轻量运行时设施：`Any`、类型转换（`cast.hpp`）、平台识别、栈帧采集、动态库加载
- 对齐 Java Math/Arrays 的无状态工具：`MathUtil`、`ArrayUtil`

core 不负责业务策略，也不包含文件系统、字符串、时间、加密等上层能力。

## 2. 依赖边界

core 的依赖方向必须保持单向：

```text
core  <-  str / fs / time / crypto / collection  <-  上层业务
```

因此 core 内部只能使用 C++ 标准库和平台系统 API。新增能力如果需要依赖 `ca::str`、`ca::fs` 等模块，应放在更高层，而不是放入 core。

## 3. 错误模型

core 以 `Result<T, E>` 作为主要错误传播机制，对齐 Rust `Result` 的“成功值或错误值”语义。设计目标是：

- 不用异常作为普通控制流。
- 成功和失败都由类型表达。
- 调用方能显式检查 `is_ok()` / `is_err()`。
- 需要组合时使用 `map`、`and_then`、`or_else` 等链式工具。

`Status` 是 `Result` 的补充，而不是替代。它提供通用错误码和消息，适合那些不需要专属错误枚举的 API。模块如果有明确领域错误，例如 fs 的 `FsError`，仍应优先使用领域错误枚举；跨模块或轻量工具可使用 `StatusResult<T>`。

## 4. 字节设施

`Bytes` 系列用于协议解析、序列化、加密输入输出等场景：

- `ByteSlice` 是非拥有只读视图，调用方负责保证底层数据生命周期。
- `Bytes` 是不可变共享字节序列，适合零拷贝切片和读游标。
- `BytesMut` 是可变缓冲区，写入后可 `freeze()` 为 `Bytes`。

字节设施显式区分大小端读写，避免把平台字节序泄漏到协议层。越界访问使用 `std::out_of_range`，因为这属于调用方违反前置条件，而不是可恢复的业务错误。

## 5. 作用域清理

`ScopeGuard` 和 `DEFER` 用于表达“离开当前作用域时执行清理”的 RAII 语义，典型场景包括：

- 临时修改状态后恢复。
- 多步骤初始化失败时回滚局部资源。
- 调用 C API 或平台 API 后保证释放句柄。

`ScopeGuard` 不可复制，只允许移动一次所有权。移动构造先转移 active 标记，再移动回调，避免移动过程中出现重复执行清理逻辑。析构函数为 `noexcept`，回调不应抛异常。

## 6. 平台与调试能力

`platform.hpp` 负责集中识别操作系统、架构和编译器，避免平台宏散落在各模块中。

`stacktrace.hpp` 提供调试辅助，不承诺在所有平台上都有同等符号质量。平台相关实现必须隔离在源文件或平台保护分支中，不能把系统头污染到公共头文件的调用方编译单元。

## 7. 动态库加载

`DynamicLibrary` 是插件宿主等场景需要的跨平台运行时加载原语：从 UTF-8 路径加载、查找导出符号、显式或自动释放原生句柄。

- **所有权**：move-only，析构调用 `unload()`，句柄始终单一所有者。`lookup<T>` 要求 `T` 为函数类型，返回的函数指针生命周期绑定库实例，`unload()` 或析构后不可调用。
- **错误模型**：失败操作返回 `StatusResult<T>`，复用既有 `Status` 码归类——`NOT_FOUND`（库文件或符号不存在）、`FAILED_PRECONDITION`（`unload()` 后或 moved-from 对象上 `lookup()`）、`INVALID_ARGUMENT`（路径/符号名为空）、`INTERNAL`（其余平台加载器错误）。
- **平台映射**：Windows 走 `LoadLibraryW`（加载前 UTF-8→UTF-16 转换）/`GetProcAddress`/`FreeLibrary`，错误码 `ERROR_FILE_NOT_FOUND` 等映射为 `NOT_FOUND`；Linux 走 `dlopen(RTLD_NOW|RTLD_LOCAL)`/`dlsym`/`dlclose`，加载前清空 `dlerror`，`lookup` 后用 `dlerror()` 而非 `dlsym` 返回值判错（`dlsym` 合法返回 null）。
- **并发**：不同实例可跨线程使用；单实例不可在 `unload()` 与 `lookup()` 间并发（加载器可能在另一线程使用符号时释放它）。

## 8. 测试策略

core 的测试位于 `libca/core/unittest/`，使用 Google Test。测试重点是：

- 值语义与移动语义是否正确。
- 错误传播是否符合 `Result` / `Status` 约定。
- 字节边界和大小端行为是否稳定。
- RAII 工具是否只执行一次，并支持取消执行。

core 是基础层，新增能力应优先补单元测试；对平台相关行为，应尽量把不可控平台条件隔离并保持 CI 可运行。

## 9. 新人阅读顺序

建议按下面顺序看代码：

1. `datatype.hpp`：理解 libca 的基础类型约定。
2. `result.hpp`：理解错误传播风格（`Result<T,E>` + `Ok`/`Err` + `TRY`）。
3. `bytes.hpp`：理解字节缓冲模型（`ByteSlice`/`Bytes`/`BytesMut`）。
4. `status.hpp`、`scope_guard.hpp`：理解通用状态返回和作用域清理工具。
5. `cast.hpp`、`any.hpp`：理解轻量 RTTI 与类型擦除。
6. `dynamic_library.hpp`、`stacktrace.hpp`、`platform.hpp`：仅在动态加载、调试或平台相关需求中阅读。
