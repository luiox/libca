---
version: 2.0
update:
2026-06-18 - 精简为稳定性冻结清单；API 详情移至头文件 Doxygen 注释
2026-06-18 - 首版（snake_case 对齐）
---

# libca/fs 稳定性冻结清单

> **API 怎么用查头文件**：`libca/fs/src/libca/fs/{file_util,path_util}.hpp`。
> **设计思路**见 `libca/fs/doc/fs设计文档.md`。本文只讲冻结状态与跨接口约定。

## 稳定性分级

| 类 | 头文件 | 稳定性 | 说明 |
|---|---|---|---|
| `PathUtil` | `path_util.hpp` | **Stable** | 纯路径字符串运算，不碰文件系统、不抛异常 |
| `FileUtil` | `file_util.hpp` | **Stable** | 文件/目录 IO |
| `FileMode` | `file_util.hpp` | **Stable** | 写入模式位标志 |

源码级兼容承诺；不承诺异常消息文本、临时文件命名的具体数值。

## 跨接口约定（用前必读）

- **字符串类型统一 `std::string`**（UTF-8 语义），不用 `ca::str`。返回路径统一 `/` 分隔符，Windows 盘符保留。
- **错误模型两类**：可能失败且需知原因 → `ca::Result<T, std::string>`；纯查询/只关心成败 → 裸 `bool` 或哨兵值（如 `size()` 失败返回 -1），不抛异常。
- **命名**：getter 无 `get_` 前缀（`size()`）、布尔查询 `is_`/`exists`、常量 UPPER_SNAKE。
- 写操作（`write_bytes`/`write_text`/`create_file`）自动创建父目录。

```cpp
#include <libca/fs/file_util.hpp>
using ca::fs::FileUtil;

auto r = FileUtil::read_all_text("config.json");
if (r.is_ok()) { /* r.unwrap() */ } else { /* r.unwrap_err() */ }
```
