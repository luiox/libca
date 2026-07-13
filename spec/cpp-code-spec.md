---
version: 1.1
update:
2026-06-18 - 明确为 libca C++ 唯一权威规范；补头文件守卫(#pragma once)、模板实现、错误模型表述
2026-05-31 - 首版
---

# libca C++ 编码规范

> 适用范围：`libca/` 下所有 C++ 模块（`core`/`str`/`fs`/`crypto`/`time`/`collection`/`utility` 等，文件后缀 `.hpp`/`.cpp`）。
> **本文件是 libca C++ 代码风格的唯一权威来源。** 与其它文档冲突时以本文为准。
>
> 不适用于：
> - **em 系列（C99）** → 见 `prompt/em_code_rule.md`（权威），`doc/01_C代码风格规范.md`、`doc/02_单元测试规范.md` 是其配套说明。em 因部分嵌入式编译器兼容性，规则与本文不同（如头文件守卫用 `#ifndef`）。
> - **libca.core/**（旧桌面代码，legacy，不约束新规范）。
>
> 设计目标：Rust 语义对齐 + 现代 C++17 实践，作为标准库的补充。

## 命名规范

| 类别 | 规范 | 示例 |
|------|------|------|
| 类型名 / 类名 | `PascalCase` | `Utf8String`, `Result<T,E>`, `FileUtil` |
| 方法 / 函数 | `snake_case` | `is_empty()`, `byte_at()`, `unwrap_or()` |
| getter | `snake_case`，**不带 `get_` 前缀** | `size()`（非 `get_size()`）, `byte_length()` |
| 布尔查询 | `is_` / `has_` / `exists` | `is_file()`, `exists()` |
| 工厂函数 / 变体构造器 | `PascalCase`（对齐 Rust） | `Ok()`, `Err()`, `from_cstr()` |
| 成员变量 | `snake_case_`（尾缀 `_`） | `data_`, `byte_length_`, `ok_` |
| 常量 / 枚举值 | `UPPER_SNAKE_CASE` | `MAX_SIZE`, `FileMode::APPEND` |
| 宏 | `UPPER_SNAKE_CASE` | `TRY(...)`, `TEST_ENABLE` |
| 命名空间 | `snake_case` | `ca::core`, `ca::str`, `ca::fs` |

## 头文件规范

- **头文件守卫统一使用 `#pragma once`**（libca 是桌面 C++，无嵌入式编译器兼容顾虑；与 em 系列的 `#ifndef` 不同）。
- **实现与声明分离**：非模板实现放 `.cpp`，头文件尽量只留声明。
  - **例外（必须留在头文件）**：模板、`constexpr` 函数、需要内联的极短访问器。`Result<T,E>`、`cast`、`any` 等模板设施天然 header-only，这是 C++ 的硬约束，不是缺陷，不要试图把模板搬进 `.cpp`。
- Include 顺序：本模块头文件 → 其他 libca 头文件 → 标准库 → 第三方库。
- 禁止在 `.cpp` 中重复 `.hpp` 的 Doxygen 注释。
- 禁止在头文件中出现 `using namespace std;`。

## API 文档（写在头文件）

- **公开 API 的使用文档写在头文件里**（Doxygen `///` 或 `/** */`，含 `@brief`/`@param`/`@return`，非平凡语义加 `@note`）。头文件是「这个函数怎么用」的唯一事实来源——调用方查头文件即得 API。
- `doc/` 下只保留**设计文档**（为什么这么设计、模块结构、遇到问题查哪个头文件），不重复罗列 API 清单。
- 设计文档遵循 `doc/00_文档规范.md` 的 YAML 头（version + update）。

## 类型使用

- 使用 `libca/core/datatype.hpp` 的定长类型：`i8/u8/i16/u16/i32/u32/i64/u64/usize/f32/f64`。
- 禁止裸 `int`、`long`、`size_t`（`usize` 替代）。
- **错误处理用 `Result<T, E>` 而非异常。** 两类约定：
  - 可能失败且调用方需知原因 → `Result<T, std::string>`（`is_ok/is_err/unwrap/unwrap_err`）。
  - 纯查询或只关心成败 → 裸 `bool` 或哨兵值（如 `size()` 失败返回 `-1`），不抛异常。
  - 方法签名约定见 `libca/core/doc/result-spec.md`。

## 字符串约定

- UTF-8 是内部唯一编码，类型族见 `libca/str/doc/str设计文档.md`。
- **`ca::str` 定稿前，新模块默认用 `std::string`**（UTF-8 语义）。`ca::str` 主要为一致性服务，待其稳定且有性能测试后再按一致性迁移下游，当前不在新接口上叠加 `ca::str` 依赖。

## 依赖分层

新增模块遵循单向依赖，禁止向上依赖、禁止同层循环依赖：

```
L0  core            ← 谁都能依赖，它不依赖任何 libca 模块
L1  str, collection ← 仅依赖 core
L2  fs, time, crypto← 依赖 L0/L1
L3  业务 / 上层
```

## 工程约定

- C++17 标准，xmake 构建。
- Google Test 单元测试，独立 `libca/<mod>/unittest/*_test.cpp` 文件（与 em 的源文件内联测试不同）。
- `*_unittest` target 受根 `with_tests` 开关守护（默认关）；本地跑测试用 `xmake f --with_tests=y`。
