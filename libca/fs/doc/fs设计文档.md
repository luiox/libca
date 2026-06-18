---
version: 2.0
update:
2026-06-18 - 重写为设计文档（API 移至头文件）；对齐 snake_case；修正过时表述
2026-05-29 - 首版
---

# libca::fs 设计文档

> 本文讲 **为什么这么设计**。具体 API 怎么用，查头文件：
> `libca/fs/src/libca/fs/file_util.hpp`（文件/目录操作）、`path_util.hpp`（路径字符串操作）。

## 1. 核心思想

对 C++17 `std::filesystem` 做一层**轻量、符合 libca 风格的封装**，提供类似 Java `FileUtil` 的便捷静态接口，消除重复的异常处理样板，统一到 `ca::Result` 错误模型。

设计原则：

- **零开销抽象**：内部完全基于 `std::filesystem`，不做额外缓冲或拷贝，仅在接口层提供便利。
- **错误模型统一**：可能失败的操作返回 `ca::Result<T, std::string>`；绝不会失败的纯查询直接返回裸值（见 §4.2）。
- **职责分层**：`PathUtil` 只做路径字符串运算、不碰文件系统；`FileUtil` 做文件/目录 IO。
- **平台透明**：返回路径统一用 `/` 分隔符（`generic_string()`），Windows 盘符保留。
- **字符串类型**：当前统一 `std::string`（UTF-8 语义），不用 `ca::str`。待 `ca::str` 定稿且有性能测试后再按一致性评估迁移。

## 2. 模块结构

```
libca/fs/
├── doc/fs设计文档.md              ← 本文件
├── src/libca/fs/
│   ├── path_util.hpp / .cpp       ← PathUtil：纯路径字符串运算
│   └── file_util.hpp / .cpp       ← FileUtil：文件/目录 IO
├── unittest/                      ← Google Test
│   ├── path_util_test.cpp
│   └── file_util_test.cpp
└── xmake.lua
```

依赖：`FileUtil` 内部用 `PathUtil`（路径拼接/归一化）与 `std::filesystem`；模块依赖 `libca_core`（`datatype.hpp`、`result.hpp`）。命名空间 `ca::fs`。

## 3. 接口概览（详细签名见头文件）

- **PathUtil**（不碰文件系统、不抛异常）：`normalize` / `to_unix_separators` / `join` / `extension` / `stem` / `filename` / `parent` / `is_absolute` / `to_absolute` / `split`。
- **FileUtil**（静态方法，按职责分组）：
  - 读写：`read_all_bytes` / `read_all_text` / `write_bytes` / `write_text`
  - 查询：`size` / `exists` / `is_file` / `is_directory` / `is_readable` / `is_writable`
  - 遍历：`list_files` / `list_entries`
  - 拷贝移动删除：`copy` / `move` / `remove` / `remove_all`
  - 创建：`create_file` / `create_directories` / `create_temp_file` / `create_temp_directory`
  - 备份：`backup`
- **FileMode** 写入模式位标志：`OVERWRITE` / `APPEND` / `CREATE_NEW`。

命名遵循 libca C++ 规范：snake_case，getter 无 `get_` 前缀（如 `size()`），常量 UPPER_SNAKE。

## 4. 关键设计决策

### 4.1 为什么 PathUtil 与 FileUtil 分开

路径字符串运算（纯函数、可全覆盖测试、永不失败）和文件 IO（有副作用、依赖环境、可能失败）是两种性质完全不同的操作。分开后 `PathUtil` 可以无副作用地被任意复用和单测。

### 4.2 错误处理策略

| 操作类别 | 返回 | 理由 |
|---|---|---|
| 纯查询（`exists`/`is_file`/`is_readable`…） | 裸 `bool` | 调用者只关心是/否，底层 OS 错误吞掉即可 |
| 大小查询（`size`） | `i64`，失败 `-1` | 单一哨兵值足够表达失败 |
| IO 操作（`read`/`write`/`copy`/`list`…） | `Result<T, std::string>` 或 `bool` | 用 `try-catch(std::exception)` 包裹，转成 Result/false，绝不向外抛异常 |

### 4.3 写操作自动建父目录

`write_bytes` / `write_text` / `create_file` 在目标父目录不存在时自动创建，省去调用方先 `create_directories` 的样板。

### 4.4 与 Java FileUtil 的差异

| Java | libca 设计 | 原因 |
|---|---|---|
| `readResource()` / `copyResource()` | 不提供 | C++ 无标准资源系统 |
| `readStreamToBytes(InputStream)` | 不提供 | 属 IO 流职责，未来归 `ca::io` |
| `byte[]` | `std::vector<u8>`（`ByteVector`） | C++ 等价 |

## 5. 未纳入本期（未来可扩展）

- 文件监控（`FileWatcher`）、文件锁（`FileLock`）
- 临时文件/目录的 RAII 守卫（当前测试里自行实现了 `TempDirGuard`）
- 文件属性精细读写（时间戳、权限位）

## 6. 测试策略

Google Test，`libca/fs/unittest/`。覆盖正常路径、空文件、不存在路径、嵌套目录遍历等；IO 测试用临时目录、结束清理。不做跨平台 mock——CI 在 Windows + Linux 分别跑。当前 80 个用例全绿。
