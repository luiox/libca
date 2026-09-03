---
version: 2.1
update:
2026-07-06 - 补充原子写入、目录拷贝、glob、元数据与权限的设计说明；更新错误模型为 FsError
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
- **错误模型统一**：可能失败且调用方需要原因的操作返回 `ca::Result<T, FsError>`；只关心成败的便捷操作返回裸 `bool` 或哨兵值（见 §4.2）。
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

依赖：`FileUtil` 内部用 `PathUtil`（路径拼接/归一化）与 `std::filesystem`；模块依赖 `libca_core`（`datatype.hpp`、`bytes.hpp`、`result.hpp`）。命名空间 `ca::fs`。

## 3. 接口概览（详细签名见头文件）

- **PathUtil**（不碰文件系统、不抛异常）：`normalize` / `to_unix_separators` / `join` / `extension` / `stem` / `filename` / `parent` / `is_absolute` / `to_absolute` / `split`。
- **FileUtil**（静态方法，按职责分组）：
  - 读写：`read_all_bytes` / `read_all_text` / `write_bytes` / `write_text` / `atomic_write_bytes` / `atomic_write_text` / `read_lines`
  - 查询：`size` / `exists` / `is_file` / `is_directory` / `metadata` / `permissions` / `is_readable` / `is_writable`
  - 遍历：`list_files` / `list_entries` / `glob`
  - 拷贝移动删除：`copy` / `copy_dir` / `move` / `remove` / `remove_all`
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
| IO 操作（`read`/`write`/`copy_dir`/`list`…） | `Result<T, FsError>` 或 `bool` | 用 `try-catch(std::exception)` 包裹，转成 Result/false，绝不向外抛异常 |

### 4.3 写操作自动建父目录

`write_bytes` / `write_text` / `create_file` 在目标父目录不存在时自动创建，省去调用方先 `create_directories` 的样板。

### 4.4 原子写入的失败语义

`atomic_write_bytes` / `atomic_write_text` 采用“同目录临时文件 + rename 提交”的设计：

1. 先在目标文件同目录生成临时文件，保证临时文件和目标位于同一文件系统。
2. 内容完整写入临时文件后，使用 `std::filesystem::rename` 作为唯一提交点。
3. 任何失败路径都只清理临时文件，不主动删除、截断或替换旧目标。

这个语义适合配置、缓存、状态文件等“新内容写失败时旧内容必须继续可用”的场景。它不试图跨文件系统移动，也不提供多进程锁；需要锁语义时应由更高层组合文件锁或进程级同步。

### 4.5 目录拷贝与符号链接

`copy_dir` 手动递归遍历源目录，而不是直接把 `std::filesystem::copy` 暴露给调用方。这样可以统一错误模型，并明确符号链接策略：

- 普通目录创建目录。
- 普通文件调用 `copy_file`。
- 符号链接复制链接本身，不跟随链接内容。
- `overwrite=true` 时会先移除目标路径本身，因此 broken symlink 也能被正确替换。

这个策略避免递归拷贝意外穿透符号链接目录，也更符合基础库“行为可预期”的目标。

### 4.6 Glob 的遍历策略

`glob` 先把模式归一化为 `/` 分隔，再转成正则匹配路径。遍历根目录由第一个通配符之前的稳定路径前缀决定。

遍历分两类：

- 模式只在文件名段使用 `*` / `?` 时，仅扫描根目录一层，例如 `dir/*.txt`。
- 模式包含 `**` 或目录段通配时递归扫描，例如 `dir/**/*.txt`、`dir/*/config.json`。

这样保留常用 glob 语义，同时避免简单模式在大型目录树上无意义地递归全量遍历。遍历时使用 `skip_permission_denied`，尽量让无权限子目录不影响其它匹配结果。

### 4.7 与 Java FileUtil 的差异

| Java | libca 设计 | 原因 |
|---|---|---|
| `readResource()` / `copyResource()` | 不提供 | C++ 无标准资源系统 |
| `readStreamToBytes(InputStream)` | 不提供 | 属 IO 流职责，未来归 `ca::io` |
| `byte[]` | `std::vector<u8>`（`ByteVector`） | C++ 等价 |

### 4.8 Path 路径值类型与编码边界

**动机**：`std::filesystem` 的窄字符接口（`path(std::string)` 构造、`.string()`）在
Windows 上按本地代码页（ACP）解析，中文/非 ASCII 路径有损。此前的对策是在每个调用点
用 `u8path`/`generic_u8string`，但：契约只在注释里、`u8path` 在 C++20 起废弃
（`char8_t` 导致返回 `std::u8string`，编译失败）、每次 API 调用重复转换、非法 UTF-8
的行为是实现定义的（MSVC 有损替换、libstdc++ 近似透传，两平台不一致）。

**设计**：新增 `Path` 值类型，内部持有 `std::filesystem::path`（Windows 即 UTF-16、
POSIX 即字节串），编码语义全部借道 `ca::str::OsString`，构造/导出命名与其对齐：

- `from_utf8`（显式校验，非法返回 `FsError::InvalidUtf8`，对齐 process 模块的
  fail-fast 边界）/ `from_utf8_lossy`（U+FFFD 替代，绝不失败）；
- `from_os_string`（POSIX 零转换）/ `from_native`（Windows 宽字符，无校验，
  可承载未配对代理等无法映射 UTF-8 的名字）；
- 导出 `to_utf8_lossy`（generic 格式，'/' 分隔）/ `to_os_string` / `native()`
  （直接喂 `std::filesystem` 与 fstream 的 `filesystem::path` 构造）。

与 `std::string` 之间**不做隐式转换**——裸字符串不携带编码信息，必须经工厂显式表达
编码语义，这是把「UTF-8 契约」从注释搬进类型系统的核心手段。

**FileUtil/PathUtil 的双轨**：每个接受路径的 FileUtil 函数提供 `const Path&` 重载
（返回路径的重载族相应返回 `Path`/`std::vector<Path>`）；`std::string` 重载族保留并
委托 `Path::from_utf8_lossy`——非法 UTF-8 从「实现定义的静默损坏」**收敛为定义清楚的
U+FFFD 替换**，不抛异常、行为跨平台一致。严格校验由调用方用 `Path::from_utf8` 显式选择。
PathUtil 保留（一次性字符串运算场景仍有用），内部实现已改经 Path。

**收敛效果**：`std::filesystem::u8path`/`generic_u8string` 在全库 src 中清零
（fs/csv/ini/json/toml/xml/yaml 的文件流统一经 `fs::Path::native()` 打开），
UTF-8 ↔ 原生编码的转换实现只存在于 `path.cpp` 一处——C++20 迁移时只需改一个文件。
解析器模块（csv/ini/json/toml/xml/yaml）因此新增对 `libca_fs` 的依赖。

**已知限制**：Windows 文件名含未配对代理（NTFS 允许）时无法表示为合法 UTF-8，
`to_utf8_lossy` 按 U+FFFD 替代；无损承载只能停留在 `Path`/`from_native` 层。
这与 Rust 用 WTF-8 解决该问题的取舍不同——只要后端是 std::filesystem，此层限制
不可避免，选择文档声明而非重造路径解析。

## 5. 未纳入本期（未来可扩展）

- 文件监控（`FileWatcher`）、文件锁（`FileLock`）
- 临时文件/目录的 RAII 守卫（当前测试里自行实现了 `TempDirGuard`）
- 文件属性写入（修改时间、权限位设置）

## 6. 测试策略

Google Test，`libca/fs/unittest/`。覆盖正常路径、空文件、不存在路径、嵌套目录遍历等；IO 测试用临时目录、结束清理。不做跨平台 mock——CI 在 Windows + Linux 分别跑。Path 另有编码边界专项用例（非法 UTF-8 的 from_utf8 拒绝 / lossy 替换、中文往返、Windows 分隔符归一、未配对代理 lossy 导出、hash/比较）。当前 140 个用例全绿（1 个 symlink 用例在 Windows 按支持情况跳过）。
