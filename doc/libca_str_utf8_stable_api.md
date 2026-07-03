---
version: 2.0
update:
2026-06-18 - 精简为稳定性冻结清单；API 详情移至头文件 Doxygen 注释
2026-06-17 - 新增 Utf8StringPool (Provisional)
2026-06-13 - 首版
---

# libca/str UTF-8 稳定性冻结清单

> **API 怎么用查头文件**：`libca/str/src/libca/str/{utf8_string,utf8_string_arena,utf8_string_pool}.hpp`。
> **本文只讲冻结状态与跨接口约定**。设计细节见各头文件内注释 + `libca/str/doc/`。

## 稳定性分级

| 类型/接口 | 头文件 | 稳定性 | 说明 |
|---|---|---|---|
| `Utf8StringRef` | `utf8_string.hpp` | **Stable** | 非拥有视图，生命周期由拥有者约束 |
| `Utf8String` | `utf8_string.hpp` | **Stable** | 拥有所有权，禁隐式拷贝、须显式 clone() |
| `Utf8Iterator` | `utf8_string.hpp` | **Stable** | 码点迭代器 |
| 非成员比较、split、join、流输出、hash | `utf8_string.hpp` | **Stable** | 跨类型内容比较，hash 用 FNV-1a |
| `Utf8StringArena` | `utf8_string_arena.hpp` | **Stable** | 追加式池（整批共存、一起释放） |
| `Utf8StringBuilder` | `utf8_string.hpp` | Provisional | 可变构建器，暴露缓冲/容量语义 |
| `ZUtf8StringRef` | `utf8_string.hpp` | Provisional | 零结尾字符串视图（字面量优化） |
| `Utf8StringPool` | `utf8_string_pool.hpp` | Provisional | 引用计数池（各自寿命、真删） |
| `Utf8StringPooledPtr` | `utf8_string_pool.hpp` | Provisional | 池化句柄，8 字节 |

源码级兼容承诺（Stable）；Provisional = 接口形状稳定但需充分验证。

## 跨接口约定（用前必读）

### 所有权与生命周期

- **Utf8String**：拥有所有权，内部 `\0` 终止，提供 `c_str()`。禁隐式拷贝，须显式 `clone()`。
- **Utf8StringRef**：非拥有视图。**不保证 `\0` 终止**（中段切片不能假设），故不提供 `c_str()`。
  - 从 `Utf8String::ref()/slice()` 派生 → 原串销毁或移动后失效。
  - 从 `Utf8StringArena::intern()` 派生 → arena 析构/clear()/移动赋值后失效。
  - 从 `Utf8StringPooledPtr::ref()` 派生 → PooledPtr 析构后失效（借用纪律）。
- **Utf8StringArena vs Utf8StringPool**：
  - arena = 同生共死，整批字符串有共同死亡点，一起释放。
  - pool = 各自寿命，按引用计数真删。Pool outlive PooledPtr 是软契约（性能优，违反走 disown fail-safe 不 UAF）。

### UTF-8 语义

- **length() = 码点数**（O(1)，构造时缓存）；**byte_length() = 字节数**；通过 `code_point_at()` 按码点访问是 O(n)（未提供 `[]` 运算符以避免低效的随机访问）。
- **切片须在码点边界**（否则未定义行为）；查找/比较/前后缀按 UTF-8 字节序列，不做 Unicode 规范化。
- 构造时校验 UTF-8 合法性（非法抛 `std::runtime_error`）。`Utf8StringRef::from_data()` 与 `ZUtf8StringRef::from_static()` 不重复校验（由调用方保证）。

### 选型指南

```cpp
Utf8String name("你好");              // 长期持有
Utf8StringRef first = name.slice_by_cp(0, 1);  // 临时切片

Utf8StringArena arena;
Utf8StringRef key = arena.intern("config.name");  // 批量解析、整体释放

Utf8StringPool pool;
auto s1 = pool.intern("hello");  // 各自寿命、真删
```

- 长期持有 → `Utf8String`；临时参数/切片 → `Utf8StringRef`。
- 批量解析、整体释放 → `Utf8StringArena`；寿命不一致 → `Utf8StringPool`。
- C API 交互只对 `Utf8String` 用 `c_str()`，不要对 `Utf8StringRef` 假设 `\0`。
