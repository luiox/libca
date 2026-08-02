# libca_random

高层随机数接口。命名空间 `ca::random`，构建目标 `libca_random`。

底层随机源复用 `ca::crypto::secure_random_bytes`（系统 CSPRNG），不实现伪随机数引擎，
不提供可复现种子。所有函数线程安全：熵池为 `thread_local`，无共享可变状态、无锁竞争。

## 用法

```cpp
#include <libca/random/random.hpp>

using namespace ca::random;

unsigned char buf[32];
fill_bytes(buf, sizeof(buf));          // 系统熵填充

u64 x = next(100);                    // [0, 100)
u64 y = range(1000, 2000);            // [1000, 2000)
double p = probability();             // [0.0, 1.0)

std::string hex = hex_string(16);     // 32 字符小写十六进制
std::string tok = alphanumeric_string(8);  // 如 "aB3x9Kq2"
```

## 接口

| 函数 | 说明 |
|------|------|
| `fill_bytes(buf, len)` | 用系统 CSPRNG 填充缓冲区；失败抛 `std::runtime_error` |
| `next(n)` | [0, n) 无偏随机整数；拒绝采样消除模偏差 |
| `range(lo, hi)` | [lo, hi) 无偏随机整数 |
| `probability()` | [0.0, 1.0) 均匀随机浮点（53 位尾数） |
| `hex_string(len)` | len 字节熵 → 2*len 小写十六进制字符 |
| `alphanumeric_string(len)` | len 个 [0-9a-zA-Z] 随机字符 |

## 设计

- **无偏整数**：`next(n)` 用拒绝采样，接受域是 n 的整数倍，n 为 2 的幂时零拒绝。
- **熵池**：内部维护 `thread_local` 熵池（每次 256 字节/系统调用），避免每个随机数
  都触发一次系统调用，显著降低长字符串生成的开销。
- **错误处理**：系统随机源失败属致命错误，统一抛 `std::runtime_error`。

## 依赖

`libca_core`、`libca_crypto`。
