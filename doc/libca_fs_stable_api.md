---
version: 3.0
update:
2026-07-03 - 破坏性变更：FileUtil 字节读写改用 ca::core::Bytes/ByteSlice；错误类型 Result<T,std::string> → Result<T,FsError>；write_bytes/write_text 由 bool 改为 Result
2026-06-18 - 精简为稳定性冻结清单；API 详情移至头文件 Doxygen 注释
2026-06-18 - 首版（snake_case 对齐）
---

# libca/fs 稳定性冻结清单

> **API 怎么用查头文件**：`libca/fs/src/libca/fs/{file_util,path_util,fs_error}.hpp`。
> **设计思路**见 `libca/fs/doc/fs设计文档.md`。本文只讲冻结状态与跨接口约定。

## 稳定性分级

| 类 | 头文件 | 稳定性 | 说明 |
|---|---|---|---|
| `PathUtil` | `path_util.hpp` | **Stable** | 纯路径字符串运算，不碰文件系统、不抛异常 |
| `FileUtil` | `file_util.hpp` | **Stable** | 文件/目录 IO |
| `FileMode` | `file_util.hpp` | **Stable** | 写入模式位标志 |
| `FsError` | `fs_error.hpp` | **Stable** | 文件操作错误码枚举 + `to_string` |

源码级兼容承诺；不承诺错误码字符串的具体文本、临时文件命名的具体数值。

## 跨接口约定（用前必读）

- **字符串类型统一 `std::string`**（UTF-8 语义），不用 `ca::str`。返回路径统一 `/` 分隔符，Windows 盘符保留。
- **字节类型**：读全文件返回 `ca::core::Bytes`（不可变、自管理生命周期）；写入接受 `ca::core::ByteSlice`（非拥有只读视图）。不再使用 `std::vector<u8>`。
- **错误模型两类**：可能失败且需知原因 → `ca::Result<T, FsError>`（`FsError` 是结构化错误码，`to_string(FsError)` 转可读字符串）；纯查询/只关心成败 → 裸 `bool` 或哨兵值（如 `size()` 失败返回 -1）。不抛异常。
- **命名**：getter 无 `get_` 前缀（`size()`）、布尔查询 `is_`/`exists`、常量 UPPER_SNAKE。
- 写操作（`write_bytes`/`write_text`/`create_file`）自动创建父目录。

```cpp
#include <libca/fs/file_util.hpp>
using ca::fs::FileUtil;

auto r = FileUtil::read_all_text("config.json");
if (r.is_ok()) { /* r.unwrap() */ }
else { /* ca::fs::to_string(r.unwrap_err()) 打印原因 */ }
```
