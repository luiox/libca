# libca_uuid

UUID v4 生成与校验。命名空间 `ca::uuid`，构建目标 `libca_uuid`。

底层随机源复用 `ca::crypto::secure_random_bytes`（系统 CSPRNG），不做伪随机数引擎，
不做 v1/v3/v5（时间戳/命名空间哈希），也不提供 UUID 结构体类型——只返回格式化字符串。

## 用法

```cpp
#include <libca/uuid/uuid.hpp>

using ca::uuid::v4;
using ca::uuid::is_valid;

std::string id = v4();                 // "550e8400-e29b-41d4-a716-446655440000"
is_valid(id);                          // true（校验格式 + v4 version/variant 位）
is_valid("00000000-0000-0000-0000-000000000000");  // false：nil 不是 v4
is_valid("00000000-0000-0000-0000-000000000000", false);  // true：宽松模式只校验格式
```

## 接口

| 函数 | 说明 |
|------|------|
| `v4()` | 生成随机 UUID v4（36 字符小写十六进制）；系统随机源失败抛 `std::runtime_error` |
| `nil()` | 全零 UUID `00000000-0000-0000-0000-000000000000` |
| `is_valid(s, check_variant_version=true)` | 校验格式；严格模式额外校验第 3 段首位为 `4`、第 4 段首位为 `8/9/a/b` |

## 依赖

`libca_core`、`libca_crypto`。
