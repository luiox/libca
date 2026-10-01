---
version: 1.0
update:
2026-10-01 - 首版：补充模块定位、系统熵/确定性原语两层结构、morpher 收敛基准与对拍策略
---

# libca/random 设计文档

## 定位

`libca_random` 提供随机数能力，分两层：

- **系统熵**（`random.hpp`）：封装 `ca::crypto::secure_random_bytes`（系统 CSPRNG），
  提供填充、无偏整数、区间、浮点、hex/字母数字字符串等便利出口。安全敏感场景用这层。
- **确定性原语**（`split_mix64.hpp`、`xorshift32.hpp`）：header-only 伪随机数生成器，
  同 seed 同序列、逐位可复现。可复现构建 / 跨仓对齐场景用这层。

两层互不依赖：确定性原语只依赖 `libca_core`（`datatype.hpp` 的 u32/u64），不接触
crypto；模块整体依赖仍为 `libca_core` + `libca_crypto`，无新增模块依赖。

## 确定性原语与 morpher 收敛基准（关键约束）

SplitMix64 / Xorshift32 不是"再实现一遍教科书算法"，而是 luiox/morpher#1027
A3/A4 的收敛单份：morpher 侧（mj2x）同名副本的删除与改引本仓是 v2.3 治理批次
（#1027 批次 3）的既定目标，届时以本仓为唯一事实来源。本仓先行落地，算法语义
**逐位锁定** morpher 正典：

| 原语 | 正典来源 | 锁定语义 |
|------|----------|----------|
| `SplitMix64` | mj2x `vmc4_instance.cpp` 内联 `SplitMix64`（A3 收敛来源） | `state_ += 0x9E3779B97F4A7C15`；`z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9`；`z = (z ^ (z >> 27)) * 0x94D049BB133111EB`；返回 `z ^ (z >> 31)` |
| `Xorshift32` | mj2x `regc_cache_pool.cpp` 的 `regcStringXorshift32`（A4 收敛来源） | `x ^= x << 13; x ^= x >> 17; x ^= x << 5`（u32 回绕即 32 位掩码语义） |

Xorshift32 在 morpher 内有两处用法（regc 链路的 `regcStringXorshift32` 与 vmc4 链路
引用的 `native_lib_crypto.hpp::xorshift32Step`），已逐位核对为**同一公式**，收敛无歧义；
类注释以 regc 版为收敛基准并注明另一处同式。

**任何常数、移位方向、混合顺序的改动都属不兼容变更**：会破坏 morpher 收敛后的
逐位一致承诺，必须与 morpher 侧同步并走 CHANGELOG。

## 位精确对拍策略

unittest（`unittest/deterministic_test.cpp`）的核心是 golden 向量对拍：

- 向量由 Python **逐位复刻 morpher 正典 C++** 独立产出（非被测实现自证），覆盖
  SplitMix64 seed 0/1/0xDEADBEEF 与 Xorshift32 seed 1/0xDEADBEEF 各 8 步。
- Xorshift32 seed 0 恒产出 0（状态是不动点）也入测锁定——这是定义行为（正典亦然，
  morpher 各链路由调用方回退固定种子），防止后续被当作 bug"无意修复"。
- `next_bounded` 有专项用例：值域 [0, bound)、bound == 1 恒 0，且严格等于同序列
  `next() % bound`（锁定正典取模公式而非任何"更均匀"的替代公式）。
- 拷贝语义：值类型可拷贝，拷贝与原对象同状态同序列且推进互不串扰。

## 语义取舍

- **`next_bounded` 的模偏差**：与正典一致直接取模，bound 非 2 的幂时分布有轻微
  偏差。这是刻意保留——morpher 侧只要求"同 seed 同序列"的可复现性，分布形状
  不是收敛契约；换成无偏采样反而破坏对拍。系统熵层的 `next(n)`（拒绝采样）才是
  无偏出口，两层的分工是清晰的。
- **bound == 0**：正典（vmc4 的 `nextBounded`）对 bound == 0 是除零 UB。本仓按
  模块惯例补 `assert` + `std::invalid_argument` 异常兜底（Release 下断言被剥离时
  有异常兜底）。对合法 bound 输出逐位不变，不触碰收敛契约。
- **Xorshift32 seed 0 不做隐式替换**：正典不替换（0 → 恒 0 序列），替换策略归
  调用方（morpher 各链路已有自己的 0 回退约定）。libca 若悄悄回退固定种子，会把
  "坏种子"静默变成"看起来正常"，掩盖调用方 bug。
- **`Xorshift32::next()` 返回推进后的完整状态**：与正典 step 函数同值（正典用法即
  `x = step(x)`，返回值直接当随机数用），调用方按需自行掩码取低字节。

## 单元测试

Google Test，`unittest/*_test.cpp`，target `libca_random_unittest`（`--with_tests=y`
时生成）。既有 `random_test.cpp` 覆盖系统熵层；`deterministic_test.cpp` 覆盖确定性
原语对拍。

## 新人阅读顺序

1. `random.hpp`：系统熵层 API 与错误语义。
2. `split_mix64.hpp`、`xorshift32.hpp`：确定性原语与收敛基准注释。
3. `unittest/deterministic_test.cpp`：golden 向量与逐位锁定方式。
