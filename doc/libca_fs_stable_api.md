---
version: 1.0
update:
2026-06-18 - libca/fs 接口对齐 snake_case 命名规范，首版冻结
---

# libca/fs 稳定接口文档

## 1. 稳定性结论

`libca/fs` 提供桌面端文件与路径操作，封装 `std::filesystem`，对外只暴露两个工具类的静态方法。

| 类 | 头文件 | 稳定性 | 结论 |
|---|---|---|---|
| `PathUtil` | `libca/fs/path_util.hpp` | Stable | 可以固定，纯字符串运算，不触碰文件系统 |
| `FileUtil` | `libca/fs/file_util.hpp` | Stable | 可以固定 |
| `FileMode` | `libca/fs/file_util.hpp` | Stable | 写入模式位标志 |

稳定承诺是源码级兼容：下游可依赖类型名、函数名、参数、返回值和已写明的行为。不承诺异常消息文本、临时文件命名的具体数值。

## 2. 选型与契约（必读）

这一节是 AI / 调用方最容易出错的地方，先读这里再用接口。

- **字符串类型**：本模块统一用 `std::string`（UTF-8 编码语义），不用 `ca::str`。路径分隔符在所有返回值里统一为 `/`（正斜杠），Windows 盘符保留。`ca::str` 完善并有性能测试后才会考虑按一致性迁移，当前不要在 fs 上叠加。
- **错误模型分两类**：
  - 可能失败且调用方需要知道原因的操作，返回 `ca::Result<T, std::string>`（如读文件、列目录、建临时文件、备份）。用 `is_ok()` / `is_err()` 判定，`unwrap()` / `unwrap_err()` 取值。
  - 纯查询或结果只有成败的操作，返回裸 `bool` 或哨兵值（如 `exists` 返回 `bool`，`size` 失败返回 `-1`）。这类**不抛异常**。
- **`PathUtil` 全部不触碰文件系统**，纯字符串运算，输入不存在的路径也安全。
- **`FileUtil` 写操作会自动创建父目录**（`write_bytes` / `write_text` / `create_file`）。
- **命名约定**：getter 不带 `get_` 前缀（`size` 而非 `get_size`），布尔查询用 `is_` / `exists`，与全库 snake_case 规范一致。

## 3. PathUtil

头文件：`libca/fs/path_util.hpp`，命名空间：`ca::fs`。全部为 `static` 方法，均不抛异常。

| 方法 | 签名 | 说明 |
|---|---|---|
| `normalize` | `std::string(const std::string&)` | 统一分隔符为 `/` 并词法归一化（消除冗余 `.` / `..`） |
| `to_unix_separators` | `std::string(const std::string&)` | 仅把 `\` 换成 `/`，不做其它归一化 |
| `join` | `std::string(base, part1)` / `(base, part1, part2)` | 拼接路径段，自动处理分隔符；第二段为绝对路径时覆盖 base |
| `extension` | `std::string(const std::string&)` | 扩展名含 `.`（如 `.txt`），无扩展名返回空串；隐藏文件 `.gitignore` 视为无扩展名 |
| `stem` | `std::string(const std::string&)` | 去扩展名的文件名；`archive.tar.gz` → `archive.tar` |
| `filename` | `std::string(const std::string&)` | 含扩展名的文件名 |
| `parent` | `std::string(const std::string&)` | 父目录；相对单段路径返回空串，根路径返回自身 |
| `is_absolute` | `bool(const std::string&)` | 是否绝对路径（平台相关：POSIX 看前导 `/`，Windows 看盘符） |
| `to_absolute` | `std::string(const std::string&)` | 基于当前工作目录解析为绝对路径 |
| `split` | `std::vector<std::string>(const std::string&)` | 拆分为各段；POSIX 下绝对路径首元素为根 `/` |

## 4. FileMode

头文件：`libca/fs/file_util.hpp`。写入模式位标志，可按位组合：

| 常量 | 值 | 含义 |
|---|---|---|
| `FileMode::OVERWRITE` | `0x01` | 覆盖写入（默认） |
| `FileMode::APPEND` | `0x02` | 追加写入 |
| `FileMode::CREATE_NEW` | `0x04` | 仅当文件不存在时创建，已存在则失败返回 `false` |

## 5. FileUtil

头文件：`libca/fs/file_util.hpp`，命名空间：`ca::fs`。全部为 `static` 方法。`ByteVector` = `std::vector<ca::u8>`。

### 5.1 读写

| 方法 | 返回 | 说明 |
|---|---|---|
| `read_all_bytes(path)` | `Result<ByteVector, std::string>` | 读整个文件为字节数组；文件不存在/非普通文件返回 `Err` |
| `read_all_text(path)` | `Result<std::string, std::string>` | 读整个文件为 UTF-8 字符串 |
| `write_bytes(path, content, mode=OVERWRITE)` | `bool` | 按模式写字节；自动创建父目录 |
| `write_text(path, content, mode=OVERWRITE)` | `bool` | 按模式写字符串 |

### 5.2 查询（不抛异常）

| 方法 | 返回 | 说明 |
|---|---|---|
| `size(path)` | `ca::i64` | 文件大小（字节）；不存在或出错返回 `-1` |
| `exists(path)` | `bool` | 路径是否存在 |
| `is_file(path)` | `bool` | 是否普通文件 |
| `is_directory(path)` | `bool` | 是否目录 |
| `is_readable(path)` | `bool` | 是否可读（不存在返回 `false`） |
| `is_writable(path)` | `bool` | 是否可写（不存在返回 `false`） |

### 5.3 遍历

| 方法 | 返回 | 说明 |
|---|---|---|
| `list_files(dir, recursive=false)` | `Result<std::vector<std::string>, std::string>` | 列目录下文件；`recursive=true` 递归子目录（不含目录本身） |
| `list_entries(dir)` | `Result<std::vector<std::string>, std::string>` | 列目录下直接条目（文件+子目录） |

### 5.4 拷贝 / 移动 / 删除

| 方法 | 返回 | 说明 |
|---|---|---|
| `copy(src, dst, overwrite=true)` | `bool` | 拷贝文件或目录（递归）；自动建目标父目录 |
| `move(src, dst, overwrite=true)` | `bool` | 移动/重命名；`src==dst` 时安全返回 `true` |
| `remove(path)` | `bool` | 删除文件或空目录 |
| `remove_all(path)` | `bool` | 递归删除；路径不存在视为已删除返回 `true` |

### 5.5 创建

| 方法 | 返回 | 说明 |
|---|---|---|
| `create_file(path)` | `bool` | 建空文件（自动建父目录）；已存在返回 `false`，不截断 |
| `create_directories(path)` | `bool` | 递归建目录；已存在视为成功 |
| `create_temp_file(prefix="", suffix="")` | `Result<std::string, std::string>` | 在系统临时目录建临时文件，返回完整路径 |
| `create_temp_directory(prefix="")` | `Result<std::string, std::string>` | 在系统临时目录建临时目录，返回完整路径 |

### 5.6 备份

| 方法 | 返回 | 说明 |
|---|---|---|
| `backup(path)` | `Result<std::string, std::string>` | 备份文件或目录，追加 `.backup` 后缀，返回备份路径 |

## 6. 推荐用法

```cpp
#include <libca/fs/file_util.hpp>
#include <libca/fs/path_util.hpp>

using ca::fs::FileUtil;
using ca::fs::PathUtil;
using ca::fs::FileMode;

// 读：返回 Result，必须判错
auto r = FileUtil::read_all_text("config.json");
if (r.is_ok()) {
    const std::string& text = r.unwrap();
} else {
    // r.unwrap_err() 是错误描述
}

// 写：返回 bool，自动建父目录
FileUtil::write_text(PathUtil::join("out", "log.txt"), "hello");
FileUtil::write_text("out/log.txt", " world", FileMode::APPEND);

// 查询：不抛异常
if (FileUtil::exists("out") && FileUtil::is_directory("out")) {
    ca::i64 n = FileUtil::size("out/log.txt");  // -1 表示失败
}
```
