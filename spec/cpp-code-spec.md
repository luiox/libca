# libca C++ 编码规范

> 本规范适用于 `libca/core`、`libca/str`、`libca/fs`、`libca/utility` 等 C++ 模块（非 em_ 系列）。
> 设计目标：Rust 语义对齐 + 现代 C++17 实践，作为标准库的补充。

## 命名规范

| 类别 | 规范 | 示例 |
|------|------|------|
| 类型名 / 类名 | `PascalCase` | `Utf8String`, `Result<T,E>`, `FileUtil` |
| 方法 / 函数 | `snake_case` | `is_empty()`, `byte_at()`, `unwrap_or()` |
| 工厂函数 / 变体构造器 | `PascalCase`（对齐 Rust） | `Ok()`, `Err()`, `from_cstr()` |
| 成员变量 | `snake_case_` (尾缀 `_`) | `data_`, `byte_length_`, `ok_` |
| 常量 / 枚举值 | `UPPER_SNAKE_CASE` | `MAX_SIZE`, `FileMode::APPEND` |
| 宏 | `UPPER_SNAKE_CASE` | `TRY(...)`, `TEST_ENABLE` |
| 命名空间 | `snake_case` | `ca::core`, `ca::str`, `ca::fs` |

## 头文件规范

- 统一使用 `#pragma once`
- Include 顺序：本模块头文件 → 其他 libca 头文件 → 标准库 → 第三方库
- 禁止在 `.cpp` 中重复 `.hpp` 的 Doxygen 注释

## 类型使用

- 使用 `libca/core/datatype.hpp` 定义的定长类型：`u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`, `i64`, `usize`
- 禁止裸 `int`、`long`、`size_t`
- 错误处理使用 `Result<T, E>` 而非异常

> Result 类型的方法签名约定参考 [`libca/core/doc/result-spec.md`](../libca/core/doc/result-spec.md)。

## 字符串约定

> 详见 [`libca/str/doc/str-spec.md`](../libca/str/doc/str-spec.md)。

## 工程约定

- C++17 标准
- 使用 xmake 构建
- Google Test 单元测试（独立 `*_test.cpp` 文件）
- 不允许在头文件中出现 `using namespace std;`
