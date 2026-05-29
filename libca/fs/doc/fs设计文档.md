---
version: 1.0
update: 2026-05-29
---

# libca::fs 设计文档

## 1. 核心思想

对 C++17 `std::filesystem` 做一层**轻量、符合 libca 风格的封装**，提供类似 Java `FileUtil` 的便捷静态接口，消除重复的异常处理样板代码，并统一使用 `ca::Result` 错误模型。

### 设计原则

- **零开销抽象**：内部完全基于 `std::filesystem`，不做额外缓冲或内存拷贝，仅在接口层提供便利。
- **错误模型统一**：可能失败的操作一律返回 `ca::Result<T, std::string>`，成功返回 `Ok(value)`，失败返回 `Err(描述)`。绝对不会抛异常的纯查询操作直接返回裸值。
- **风格对齐**：仿照 `ca::StringUtil` 的静态工具类模式，`ca::fs::FileUtil` / `ca::fs::PathUtil`，使用 `#pragma once` + `#include` guard，位于 `ca::fs` 命名空间。
- **平台透明**：内部自动处理路径分隔符差异；Windows 上自动处理前缀 `\\?\` 长路径（视需要）。
- **模块分层**：`path_util` 处理路径字符串操作，`file_util` 处理文件/目录 IO。

## 2. 模块结构

```
libca/fs/
├── doc/
│   └── fs设计文档.md          ← 本文件
├── src/
│   └── libca/
│       └── fs/
│           ├── file_util.hpp  ← FileUtil 声明
│           ├── file_util.cpp  ← FileUtil 实现
│           ├── path_util.hpp  ← PathUtil 声明
│           └── path_util.cpp  ← PathUtil 实现
├── unittest/
│   ├── file_util_test.cpp
│   └── path_util_test.cpp
└── xmake.lua
```

依赖关系：`FileUtil` 内部依赖 `PathUtil`（路径归一化等）和 `std::filesystem`。

## 3. 接口设计

### 3.1 FileUtil — 文件/目录操作

对标 Java `FileUtil`，均为 `static` 方法。

| 分组 | 方法签名 | 对标 Java | 说明 |
|------|---------|-----------|------|
| **读写** | `readAllBytes(path) → Result<ByteVector, String>` | `readAllFile()` | 读整个文件为 `std::vector<u8>` |
| | `readAllText(path, encoding?) → Result<String, String>` | — | 读整个文件为 UTF-8 字符串 |
| | `writeBytes(path, bytes, mode) → bool` | `writeFile()` | 按模式写入字节（Overwrite/Append/CreateNew） |
| | `writeText(path, text, mode) → bool` | — | 按模式写入字符串 |
| **查询** | `getSize(path) → i64` | `getFileSize()` | 文件大小，失败返回 -1 |
| | `exists(path) → bool` | — | 判断路径是否存在 |
| | `isFile(path) → bool` | — | 是否为普通文件 |
| | `isDirectory(path) → bool` | — | 是否为目录 |
| **遍历** | `listFiles(path, recursive?) → Result<PathList, String>` | `getAllFiles()` | 列出目录下所有文件 |
| | `listEntries(path) → Result<PathList, String>` | — | 列出目录下直接条目 |
| **拷贝/移动** | `copy(src, dst, overwrite?) → bool` | — | 拷贝文件或目录 |
| | `move(src, dst, overwrite?) → bool` | — | 移动/重命名 |
| **删除** | `remove(path) → bool` | `deleteFile()` | 删除文件或空目录 |
| | `removeAll(path) → bool` | — | 递归删除文件/目录 |
| **创建** | `createFile(path) → bool` | `canCreateFile()` | 创建文件（含自动创建父目录） |
| | `createDirectories(path) → bool` | — | 递归创建目录 |
| | `createTempFile(prefix, suffix?) → Result<Path, String>` | — | 创建临时文件 |
| | `createTempDirectory(prefix?) → Result<Path, String>` | — | 创建临时目录 |
| **备份** | `backup(path) → Result<Path, String>` | `backupFile()` | 备份为 `.backup`（文件/目录） |
| **权限** | `isReadable(path) → bool` | — | 是否可读 |
| | `isWritable(path) → bool` | — | 是否可写 |

### 3.2 PathUtil — 路径字符串操作

| 方法签名 | 对标 Java | 说明 |
|---------|-----------|------|
| `normalize(path) → String` | `backslashToForwardSlash()` | 统一分隔符为 `/`，去除冗余 `.`/`..` |
| `join(base, ...parts) → String` | — | 路径拼接（智能处理分隔符） |
| `extension(path) → String` | — | 获取扩展名（含 `.`） |
| `stem(path) → String` | — | 获取无扩展名的文件名 |
| `filename(path) → String` | — | 获取文件名（含扩展名） |
| `parent(path) → String` | — | 获取父目录路径 |
| `isAbsolute(path) → bool` | — | 是否为绝对路径 |
| `toAbsolute(path) → String` | — | 转为绝对路径 |

### 3.3 模式常量

```cpp
struct FileMode {
    static constexpr u32 Overwrite = 0x01;   // 覆盖写入（默认）
    static constexpr u32 Append   = 0x02;    // 追加写入
    static constexpr u32 CreateNew = 0x04;   // 创建新文件（失败若已存在）
};
```

## 4. 架构设计

### 4.1 架构概览

```
用户代码
    │
    ├── ca::fs::FileUtil::readAllText(path)
    │       │
    │       ├── PathUtil::normalize(path)   ← 路径归一化
    │       └── std::filesystem::read(...)  ← 底层 IO
    │
    └── ca::fs::PathUtil::join(a, b)
            └── std::filesystem::path::operator/=
```

### 4.2 错误处理策略

- **纯查询（exists/isFile/isReadable 等）**：直接返回 `bool`，不在 API 层面处理底层 OS 错误——调用者只关心是/否。
- **IO 操作（read/write/copy/remove 等）**：用 `try-catch(std::filesystem_error)` + `catch(std::exception)` 包裹，将异常转换为 `ca::Result<T, std::string>` 或返回 `false`。
- **路径字符串操作**：纯字符串运算，不接触文件系统，不可能失败，直接返回裸值。

### 4.3 与 Java FileUtil 差异说明

| Java 方法 | C++ 等价设计 | 差异原因 |
|-----------|-------------|---------|
| `readResource()` / `copyResource()` | **不提供** | C++ 无标准资源系统概念 |
| `readStreamToBytes(InputStream)` | **不提供** | 这属于 IO 流工具，非 filesystem 职责，可放在 `ca::io` |
| `Result<byte[], String>` | `Result<ByteVector, String>` | 用 `std::vector<u8>` 替代 `byte[]` |

### 4.4 未纳入本期设计的接口（未来可扩展）

- 文件监控（`FSEvent` / `FileWatcher`）
- 文件锁（`FileLock`）
- 临时文件/目录的 RAII 守卫
- 文件属性（时间戳、权限的精细读写）

## 5. 使用示例

```cpp
#include <libca/fs/file_util.hpp>
#include <libca/fs/path_util.hpp>
#include <iostream>

using namespace ca::fs;
using ca::Ok, ca::Err;

// 读文件
auto result = FileUtil::readAllText("data/config.json");
if (result) {
    std::cout << "content: " << *result << std::endl;
}

// 写文件
FileUtil::writeText("output.txt", "hello, world", FileMode::Overwrite);

// 递归遍历
auto files = FileUtil::listFiles("src/", true);
if (files) {
    for (auto& f : *files) {
        std::cout << f.string() << std::endl;
    }
}

// 路径操作
auto full = PathUtil::join("/base", "sub", "file.txt");
// → "base/sub/file.txt" (Linux) 或 "base\\sub\\file.txt" (Windows)
auto normalized = PathUtil::normalize("C:\\Users\\test\\..\\file.txt");
// → "C:/Users/file.txt"
```

## 6. 构建集成

`libca/fs/` 已有独立的 `xmake.lua`（待创建），采用 `headeronly` 模式（因实现简单，可全头文件；但为减小编译依赖，`.hpp` 放声明、`.cpp` 放实现）。

```lua
target("libca_fs")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/fs/*.cpp")
    add_headerfiles("src/libca/fs/*.hpp")
    add_includedirs("src", {public = true})
    add_deps("libca_core")  -- 依赖 datatype.hpp, Result.hpp
```

## 7. 测试策略

- Google Test，文件放在 `libca/fs/unittest/`
- 测试覆盖：正常路径、空文件、大文件、不存在路径、嵌套目录遍历、非法路径字符
- 使用临时目录做 IO 测试，测试结束后清理
- 不做跨平台 mock——CI 在 Windows + Linux 分别跑

## 8. 实现优先级

1. **Phase 1** — 基础类型定义 + `PathUtil`（纯字符串操作，无 IO，可全覆盖测试）
2. **Phase 2** — `FileUtil` 查询类（exists/isFile/isDirectory/getSize）+ 读写类
3. **Phase 3** — `FileUtil` 目录操作（listFiles/createDirectories/removeAll）
4. **Phase 4** — `FileUtil` 拷贝/移动/备份 + 错误处理打磨
5. **Phase 5** — xmake 构建集成 + unittest 完善
