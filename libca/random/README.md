# libca_random

随机数模块。命名空间 `ca::random`，构建目标 `libca_random`。

两层能力：

- **系统熵**（`random.hpp`）：随机源复用 `ca::crypto::secure_random_bytes`
  （系统 CSPRNG），无共享可变状态。所有函数线程安全：熵池为 `thread_local`，
  无锁竞争。
- **确定性原语**（`split_mix64.hpp`、`xorshift32.hpp`）：header-only 伪随机数
  生成器，同 seed 同序列、逐位可复现。语义是 morpher 收敛基准
  （luiox/morpher#1027 A3/A4），改动算法常数属不兼容变更。

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

确定性原语（header-only，同 seed 同序列）：

| 类 | 说明 |
|------|------|
| `SplitMix64(seed)` | 64 位确定性生成器；`next()` / `next_bounded(bound)`（= next() % bound）。收敛基准 = mj2x vmc4 版 |
| `Xorshift32(seed)` | 32 位确定性生成器；`next()` / `next_bounded(bound)`。收敛基准 = mj2x regc 版；seed 0 恒产出 0（定义行为） |

## 设计

- **无偏整数**：`next(n)` 用拒绝采样，接受域是 n 的整数倍，n 为 2 的幂时零拒绝。
- **熵池**：内部维护 `thread_local` 熵池（每次 256 字节/系统调用），避免每个随机数
  都触发一次系统调用，显著降低长字符串生成的开销。
- **错误处理**：系统随机源失败属致命错误，统一抛 `std::runtime_error`。
- **确定性原语逐位锁定**：SplitMix64 / Xorshift32 的算法常数与移位顺序与 morpher
  正典逐位一致，unittest 用 golden 向量对拍锁定；`next_bounded` 与正典一致直接
  取模（非 2 幂 bound 有轻微模偏差，不影响可复现性），仅对 bound == 0 补了
  断言 + 异常兜底（正典为 UB）。
- **依赖**：确定性原语只依赖 core（`datatype.hpp` 的 u32/u64）；模块整体仍只依赖
  `libca_core`、`libca_crypto`，无新增模块依赖。

## 依赖

`libca_core`、`libca_crypto`。
