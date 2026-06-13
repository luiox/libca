---
version: 1.0
update:
2026-06-13 - 完成 libca/core 稳定接口分级与首版冻结清单
---

# libca/core 稳定接口文档

## 1. 稳定性结论

`libca/core` 当前可以固定为源码级 API 的部分是：

| 模块 | 头文件 | 稳定性 | 结论 |
|---|---|---|---|
| 基础类型 | `libca/core/datatype.hpp` | Stable | 可以固定 |
| 字节缓冲 | `libca/core/bytes.hpp` | Stable | 可以固定 |
| Result | `libca/core/result.hpp` | Stable | 可以固定，`TRY` 需标注编译器限制 |
| Any/type_id | `libca/core/any.hpp` | Stable | 可以固定 |
| 类型转换 | `libca/core/cast.hpp` | Stable | 可以固定，语义是精确类型匹配 |
| 导出宏 | `libca/core/dllexport.hpp` | Stable | 可以固定 |
| 平台检测 | `libca/core/platform.hpp` | Experimental | 可用，但暂不建议下游强依赖宏细节 |
| 栈追踪 | `libca/core/stacktrace.hpp` | Experimental | 可用作调试工具，不建议作为业务稳定接口 |
| 单例包装 | `libca/core/wrapper.hpp` | Legacy | 不建议固定为新接口 |

稳定承诺是源码级兼容：下游可以依赖类型名、函数名、参数、返回值和已写明的行为。当前不承诺二进制 ABI、对象内存布局、私有成员、异常消息文本、哈希具体数值、内部扩容策略的精确细节。

## 2. 固定规则

- 公共命名空间以 `ca::core` 为主。
- `datatype.hpp` 中的基础类型在 `ca` 命名空间。
- `result.hpp` 当前保留 `namespace ca { using namespace ca::core; }` 的兼容导出，但新代码应显式使用 `ca::core`。
- 头文件路径按安装后的 include 形式使用：`#include <libca/core/xxx.hpp>`。
- C++ 标准为 C++17。
- 只把 public 类型、public 成员函数、非成员函数、公开宏列为稳定接口。

## 3. 基础类型

头文件：`libca/core/datatype.hpp`

命名空间：`ca`

| 名称 | 含义 |
|---|---|
| `i8` / `i16` / `i32` / `i64` | 有符号定长整数 |
| `u8` / `u16` / `u32` / `u64` | 无符号定长整数 |
| `f32` / `f64` | 单精度、双精度浮点 |
| `usize` | 大小和索引语义 |
| `byte` | 字节语义，当前等同于 `u8` |

字面量：

| 名称 | 返回类型 |
|---|---|
| `operator""_i32(unsigned long long)` | `i32` |
| `operator""_u64(unsigned long long)` | `u64` |

设计评价：基础类型是合理的全库统一入口，可以稳定。`usize` 当前依赖 `size_t`，文档层面只承诺其为大小语义类型，不承诺具体位宽。

## 4. Bytes / BytesMut

头文件：`libca/core/bytes.hpp`

命名空间：`ca::core`

### 4.1 ByteSlice

`ByteSlice` 是非拥有只读字节视图。调用方必须保证底层数据生命周期覆盖视图使用期。

| 接口 | 行为 |
|---|---|
| `ByteSlice() noexcept` | 构造空视图 |
| `ByteSlice(const u8* data, usize len) noexcept` | 从指针和长度构造 |
| `data() const noexcept -> const u8*` | 返回底层指针 |
| `size() const noexcept -> usize` | 返回字节数 |
| `empty() const noexcept -> bool` | 是否为空 |
| `operator[](usize index) const -> const u8&` | 无边界检查访问 |
| `sub_slice(usize start, usize count) const -> ByteSlice` | 返回子视图，越界抛 `std::out_of_range` |

### 4.2 Bytes

`Bytes` 是不可变字节序列，支持共享存储和读游标。

| 接口 | 行为 |
|---|---|
| `Bytes() noexcept` | 空序列 |
| `static from_static(const u8* data, usize len) -> Bytes` | 引用外部静态数据，不复制 |
| `static copy_from_slice(const u8* data, usize len) -> Bytes` | 复制数据 |
| 拷贝构造/赋值 | 浅拷贝共享存储 |
| 移动构造/赋值 | 转移对象状态 |
| `len() const noexcept -> usize` | 总长度 |
| `is_empty() const noexcept -> bool` | 是否为空 |
| `as_ptr() const noexcept -> const u8*` | 当前读位置指针 |
| `remaining() const noexcept -> usize` | 剩余可读字节 |
| `advance(usize cnt)` | 前进读游标，越界抛 `std::out_of_range` |
| `slice(usize begin, usize end) const -> Bytes` | 零拷贝切片，越界抛 `std::out_of_range` |
| `copy_to_slice(u8* dst, usize len)` | 复制并前进游标，越界抛 `std::out_of_range` |

类型化读取：

| 接口 |
|---|
| `get_u8()` |
| `get_u16_be()` / `get_u16_le()` |
| `get_u32_be()` / `get_u32_le()` |
| `get_u64_be()` / `get_u64_le()` |
| `get_i16_be()` / `get_i16_le()` |
| `get_i32_be()` / `get_i32_le()` |
| `get_i64_be()` / `get_i64_le()` |
| `get_f32_be()` / `get_f64_be()` |

后缀 `_be` 表示大端，`_le` 表示小端。读取不足时抛 `std::out_of_range`。

### 4.3 BytesMut

`BytesMut` 是可变字节缓冲区，写入追加到尾部，读取使用独立读游标。

| 接口 | 行为 |
|---|---|
| `BytesMut() noexcept` | 空缓冲 |
| `static with_capacity(usize cap) -> BytesMut` | 预分配容量 |
| 拷贝构造/赋值 | 深拷贝内容 |
| 移动构造/赋值 | 转移对象状态 |
| `len() const noexcept -> usize` | 已写入长度 |
| `is_empty() const noexcept -> bool` | 是否为空 |
| `as_ptr() const noexcept -> const u8*` | 当前读位置只读指针 |
| `as_mut_ptr() const noexcept -> u8*` | 当前读位置可写指针 |
| `remaining() const noexcept -> usize` | 剩余可读字节 |
| `remaining_mut() const noexcept -> usize` | 剩余可写容量 |
| `advance(usize cnt)` | 前进读游标，越界抛 `std::out_of_range` |
| `reserve(usize additional)` | 确保还能写入指定字节数 |
| `clear() noexcept` | 清空内容并重置读游标 |
| `truncate(usize len)` | 截断到指定长度 |
| `put_slice(const u8* data, usize len)` | 追加写入字节 |
| `freeze() -> Bytes` | 转为不可变 `Bytes`，原对象清空 |
| `equals(const BytesMut& other) const noexcept -> bool` | 比较剩余内容 |
| `operator==` / `operator!=` | 内容比较 |

类型化写入：

| 接口 |
|---|
| `put_u8(u8 val)` |
| `put_u16_be(u16 val)` / `put_u16_le(u16 val)` |
| `put_u32_be(u32 val)` / `put_u32_le(u32 val)` |
| `put_u64_be(u64 val)` / `put_u64_le(u64 val)` |
| `put_i16_be(i16 val)` / `put_i16_le(i16 val)` |
| `put_i32_be(i32 val)` / `put_i32_le(i32 val)` |
| `put_i64_be(i64 val)` / `put_i64_le(i64 val)` |
| `put_f32_be(f32 val)` / `put_f64_be(f64 val)` |

类型化读取接口与 `Bytes` 一致。

设计评价：这个模块抽象清晰，适合固定。建议后续只增加接口，不改变读游标、端序显式化和异常行为。

## 5. Result

头文件：`libca/core/result.hpp`

命名空间：`ca::core`

### 5.1 类型和工厂

| 接口 | 行为 |
|---|---|
| `template<typename T, typename E> struct Result` | 成功值 `T` 或错误值 `E` |
| `types::Ok<T>` | 成功值包装 |
| `types::Ok<void>` | 无值成功包装 |
| `types::Err<E>` | 错误值包装 |
| `Ok(T&& val)` | 构造 `types::Ok<decay_t<T>>` |
| `Ok()` | 构造 `types::Ok<void>` |
| `Err(E&& val)` | 构造 `types::Err<decay_t<E>>` |

约束：

- `E` 不能是 `void`。
- `T` 可以是 `void`。
- `Result` 当前支持拷贝和移动，前提是内部值类型满足对应构造要求。

### 5.2 成员接口

| 接口 | 行为 |
|---|---|
| `is_ok() const -> bool` | 是否为成功 |
| `is_err() const -> bool` | 是否为错误 |
| `expect(const char* msg) const -> T` | 成功则取值，失败则打印消息并 `std::terminate` |
| `unwrap() const -> T` | 成功则取值，失败则 `std::terminate` |
| `unwrap_or(const U& defaultValue) const -> U` | 成功取值，失败返回默认值 |
| `unwrap_err() const -> E` | 错误则取错误值，成功则 `std::terminate` |
| `map(Func func) const` | 转换成功值 |
| `map_error(Func func) const` | 转换错误值 |
| `then(Func func) const` | 成功时执行副作用，返回原结果 |
| `otherwise(Func func) const` | 错误时执行副作用，返回原结果 |
| `and_then(Func func) const` | 成功时链式返回另一个 `Result` |
| `or_else(Func func) const` | 错误时链式恢复为另一个 `Result` |
| `storage()` | 暴露内部存储，主要供宏和底层工具使用 |

比较：

| 接口 | 行为 |
|---|---|
| `operator==(const Result<T,E>&, const Result<T,E>&)` | 同为 Ok 比较成功值，同为 Err 比较错误值 |
| `operator==(const Result<T,E>&, types::Ok<T>)` | 与 Ok 包装比较 |
| `operator==(const Result<T,E>&, types::Err<E>)` | 与 Err 包装比较 |
| `operator==(const Result<void,E>&, const Result<void,E>&)` | void 成功只比较状态 |
| `operator==(const Result<void,E>&, types::Ok<void>)` | 判断是否成功 |

### 5.3 TRY 宏

`TRY(expr)` 用于在返回 `Result<*, E>` 的函数中展开另一个 `Result<T, E>`：

- 成功时表达式结果为内部 Ok 值。
- 失败时提前 `return types::Err<E>(...)`。

限制：

- 当前实现依赖 GNU statement expression：`__extension__ ({ ... })`。
- GCC 和 Clang 可用。
- MSVC 下不作为稳定可用接口承诺。
- 不建议在公共头文件或需要 MSVC 兼容的代码中使用 `TRY`。

设计评价：`Result` 接口已覆盖常用错误处理模式，可以固定。但 `storage()` 和 `TRY` 属于底层/便利接口，文档需明确限制。

## 6. Any / type_id

头文件：`libca/core/any.hpp`

命名空间：`ca::core`

| 接口 | 行为 |
|---|---|
| `template<typename T> constexpr const void* type_id() noexcept` | 返回类型唯一标记地址 |
| `Any() noexcept` | 空对象 |
| `Any(T&& value)` | 持有 `decay_t<T>` |
| `Any(const Any& other)` | 可拷贝值会复制；仅移动值复制后为空 |
| `Any(Any&& other) noexcept` | 移动持有值 |
| `operator=(const Any&)` | 拷贝赋值 |
| `operator=(Any&&) noexcept` | 移动赋值 |
| `reset() noexcept` | 清空 |
| `has_value() const noexcept -> bool` | 是否持有值 |
| `is<T>() const noexcept -> bool` | 是否正好持有类型 `T` |
| `as<T>() noexcept -> T*` | 类型匹配返回指针，否则 `nullptr` |
| `as<T>() const noexcept -> const T*` | const 版本 |
| `cast<T>() -> T&` | 无检查取引用，类型不匹配是未定义行为 |
| `cast<T>() const -> const T&` | const 版本 |
| `type_tag() const noexcept -> const void*` | 当前类型标记，空对象为 `nullptr` |

设计评价：不依赖 RTTI，轻量，适合固定。需要明确 `cast<T>()` 不检查类型，下游优先使用 `as<T>()`。

## 7. Polymorphic / cast

头文件：`libca/core/cast.hpp`

命名空间：`ca::core`

| 接口 | 行为 |
|---|---|
| `class Polymorphic` | 提供 `virtual const void* type_tag() const noexcept = 0` |
| `CA_TYPE_TAG(T)` | 在派生类中实现 `type_tag()` |
| `isa<T>(const U* ptr) noexcept -> bool` | 精确动态类型匹配，空指针返回 false |
| `cast<T>(U* ptr) noexcept -> T*` | 无检查转换，调用方必须先确保类型匹配 |
| `cast<T>(const U* ptr) noexcept -> const T*` | const 版本 |
| `dyn_cast<T>(U* ptr) noexcept -> T*` | 匹配返回转换指针，否则 `nullptr` |
| `dyn_cast<T>(const U* ptr) noexcept -> const T*` | const 版本 |

重要语义：

- `isa<T>` 是精确类型匹配，不表示“是否为某个基类”。
- 多级继承中，实际对象是 `Human` 时，`isa<Mammal>` 返回 false，`isa<Human>` 返回 true。
- `cast<T>` 等价于确认类型后的 `static_cast`，不做运行时检查。

设计评价：当前实现比旧文档中的 `TypeOf<T>` 方案更简单，适合固定。旧 `cast_design.md` 中提到的 `typed` 命名空间和 `getType()` 机制已过时，不应作为下游依据。

## 8. dllexport

头文件：`libca/core/dllexport.hpp`

| 宏 | 行为 |
|---|---|
| `LIBCA_API` | 静态库模式为空 |
| `LIBCA_DLL_MODE` + `LIBCA_DLL_EXPORT` | Windows 下 `__declspec(dllexport)` |
| `LIBCA_DLL_MODE` 且无 `LIBCA_DLL_EXPORT` | Windows 下 `__declspec(dllimport)` |

设计评价：可以固定。当前只覆盖 MSVC/Windows 风格导入导出语义，跨平台 visibility 后续可扩展。

## 9. 暂不固定为 Stable 的接口

### 9.1 platform.hpp

当前可用接口：

- `CA_PLATFORM_WINDOWS`
- `CA_PLATFORM_LINUX`
- `CA_COMPILER_CLANG`
- `CA_COMPILER_GCC`
- `CA_COMPILER_MSVC`
- `CA_COMPILER_UNKNOWN`
- `ca::core::get_os_name() -> std::string`

暂不固定原因：

- 头文件会引入平台系统头，影响下游编译环境。
- 当前只支持 Windows/Linux，遇到其他平台直接 `#error`。
- 编译器宏未统一保证未命中分支为 0。

建议：下游可以在调试或平台适配层使用，不建议业务接口暴露这些宏。

### 9.2 stacktrace.hpp

当前可用接口：

- `capture_stack_trace(i32 max_frames = 64) -> std::string`
- `print_stack_trace(i32 max_frames = 64) -> void`

暂不固定原因：

- 输出格式依赖操作系统、编译器、符号信息和链接参数。
- 适合调试和日志，不适合作为可解析协议或稳定业务数据。

稳定建议：只承诺函数存在和“不崩溃地返回字符串/打印”，不承诺每一行内容格式。

### 9.3 wrapper.hpp

当前可用接口：

- `ca::Singleton<T>::getInstance(args...) -> T*`
- `ca::MeyersSingleton<T>::getInstance() -> T&`

不建议固定原因：

- `Singleton` 使用手写 double-checked locking 和裸指针生命周期管理，设计风险高。
- 命名空间在 `ca` 而非 `ca::core`，与 core 新接口风格不统一。
- 新代码可直接使用函数内静态变量或明确依赖注入。

建议：保留兼容，但不作为新稳定接口推广。

## 10. 下游使用建议

```cpp
#include <libca/core/datatype.hpp>
#include <libca/core/result.hpp>
#include <libca/core/bytes.hpp>

ca::core::Result<ca::u32, std::string> parse_id(ca::core::Bytes bytes) {
    if (bytes.remaining() < 4) {
        return ca::core::Err(std::string("short input"));
    }
    return ca::core::Ok(bytes.get_u32_be());
}
```

优先使用：

- `ca::core::Result<T, E>` 表达可恢复错误。
- `ca::core::Bytes` / `BytesMut` 表达字节协议和序列化缓冲。
- `ca::core::Any` 只用于确实需要类型擦除的边界。
- `ca::core::dyn_cast<T>` 用于精确类型分派。

避免把 Experimental/Legacy 接口扩散到下游公共 API。
