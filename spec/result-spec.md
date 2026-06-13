# Result<T, E> — 类型规范

> 路径：`libca/core/src/libca/core/result.hpp`
> 对齐 Rust `std::result`，替代异常作为错误处理机制。

## 方法签名

| 方法 | Rust 对应 | 行为 |
|------|-----------|------|
| `is_ok() -> bool` | `is_ok()` | 是否持有 Ok 值 |
| `is_err() -> bool` | `is_err()` | 是否持有 Err 值 |
| `unwrap() -> T` | `unwrap()` | 取出 Ok 值，若为 Err 则 `std::terminate` |
| `unwrap_or(default) -> T` | `unwrap_or()` | 取出 Ok 值，否则返回默认值 |
| `unwrap_err() -> E` | `unwrap_err()` | 取出 Err 值，若为 Ok 则 `std::terminate` |
| `expect(msg) -> T` | `expect()` | 取出 Ok 值，若为 Err 则打印 msg + `std::terminate` |
| `map(f) -> Result<U, E>` | `map()` | Ok 值经 f 变换，Err 透传 |
| `map_error(f) -> Result<T, F>` | `map_err()` | Err 值经 f 变换，Ok 透传 |
| `then(f) -> Result<T, E>` | n/a | Ok 时执行副作用 f，原值透传 |
| `otherwise(f) -> Result<T, E>` | `inspect_err()` | Err 时执行副作用 f，原值透传 |
| `or_else(f) -> Result<T, F>` | `or_else()` | Err 时用 f 恢复 |

## 工厂函数

| 函数 | Rust 对应 | 说明 |
|------|-----------|------|
| `Ok(T&& val)` | `Ok(val)` | 构造成功变体 |
| `Err(E&& val)` | `Err(val)` | 构造错误变体 |

## 类型定义

| 类型 | 说明 |
|------|------|
| `types::Ok<T>` | Ok 值包装类型 |
| `types::Err<E>` | Err 值包装类型 |

## TRY 宏

对应 Rust 的 `?` 运算符，用于函数内提前返回错误：

```cpp
auto val = TRY(fallible_func());
// 等价于：
auto res = fallible_func();
if (!res.is_ok())
    return types::Err<E>(res.storage().get<E>());
// 继续使用 res 内的 T 值
```

## 常规用法

```cpp
Result<int, std::string> divide(int a, int b) {
    if (b == 0)
        return Err(std::string("divide by zero"));
    return Ok(a / b);
}

auto r = divide(10, 2);
if (r.is_ok()) {
    fmt::print("result: {}\n", r.unwrap());
}
```

## 设计约束

- `E` 不能为 `void`
- `T` 可为 `void`（对应 `Result<void, E>`）
- 不可复制（拷贝构造已删除），仅可移动
