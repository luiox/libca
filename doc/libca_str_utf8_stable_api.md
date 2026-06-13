---
version: 1.0
update:
2026-06-13 - 完成 utf8_string.hpp 与 utf8_string_arena.hpp 稳定接口分级与首版冻结清单
---

# libca/str UTF-8 稳定接口文档

## 1. 固定范围

本文件只固定以下两个头文件中的接口：

- `libca/str/src/libca/str/utf8_string.hpp`
- `libca/str/src/libca/str/utf8_string_arena.hpp`

其他 `libca/str` 头文件和模块暂不在本次稳定承诺内。

当前结论：

| 类型/接口 | 稳定性 | 结论 |
|---|---|---|
| `Utf8StringRef` | Stable | 可以固定 |
| `Utf8String` | Stable | 可以固定 |
| `Utf8Iterator` | Stable | 可以固定 |
| 非成员比较、`split`、`join`、流输出、hash | Stable | 可以固定 |
| `Utf8StringArena` | Stable | 可以固定，需明确生命周期和非线程安全 |
| `Utf8StringBuilder` | Provisional | 头文件内可用，但不建议本次对下游固定 |
| `ZUtf8StringRef` | Provisional | 思路合理，但缓存和生命周期语义建议单独冻结 |

稳定承诺是源码级 API，不承诺二进制 ABI、对象内存布局、私有成员、哈希具体数值、异常消息文本。

## 2. 设计合理性分析

当前设计中合理且适合固定的点：

- `Utf8String` 拥有数据且不可变，移动可用、复制必须显式 `clone()`，可以避免意外深拷贝。
- `Utf8StringRef` 是非拥有视图，适合切片、参数传递和 arena 返回值。
- `Utf8String` 保证末尾有 `\0`，因此可以提供 `c_str()`。
- `Utf8StringRef` 不保证末尾有 `\0`，因此不提供 `c_str()`，这个约束正确。
- UTF-8 构造时校验，码点长度缓存，`length()` 为 O(1)。
- 码点下标访问 O(n)，文档明确后可接受。
- `Utf8StringArena` 提供批量生命周期和去重，适合解析器、配置加载、编译前端等场景。

当前不建议固定的点：

- `Utf8StringBuilder` 目前作为内部构建工具很好用，但会暴露可变缓冲策略，建议先保留为 Provisional。
- `ZUtf8StringRef` 依赖全局缓存、外部字符串生命周期和字面量地址语义，设计方向可以保留，但稳定文档应暂不鼓励下游广泛使用。
- `Utf8StringRef::from_data()` 不校验 UTF-8，若调用方传错 `cp_len` 会污染后续码点语义；可用，但文档必须明确调用方负责。

## 3. 通用规则

- 命名空间：`ca::str`。
- 基础整数类型来自 `libca/core/datatype.hpp`。
- UTF-8 是唯一编码。
- `length()` 表示 Unicode code point 数量，不表示字节数。
- `byte_length()` 表示 UTF-8 字节数，不包含 `Utf8String` 内部的结尾 `\0`。
- 比较、查找、前后缀检查按 UTF-8 字节序列执行，不做 Unicode 规范化。
- `to_lower()` / `to_upper()` 依赖当前 `Utf8Char` 能力，不承诺完整 Unicode 大小写折叠。

## 4. Utf8StringRef

`Utf8StringRef` 是非拥有、不可变 UTF-8 字符串视图。

生命周期规则：

- `Utf8StringRef` 不拥有数据。
- 调用方必须保证 `data()` 指向的数据在视图使用期间有效。
- 来自 `Utf8String::ref()` / `slice()` 的视图在原 `Utf8String` 销毁或移动后失效。
- 来自 `Utf8StringArena::intern()` 的视图在 arena 被销毁、移动赋值覆盖或 `clear()` 后失效。

### 4.1 构造

| 接口 | 稳定性 | 行为 |
|---|---|---|
| `Utf8StringRef() noexcept` | Stable | 空视图 |
| `Utf8StringRef(const u8* data, usize byte_length, usize length) noexcept` | Stable | 从外部数据构造，不校验、不复制 |
| `Utf8StringRef(const Utf8String& str) noexcept` | Stable | 引用 `Utf8String` 内部数据 |
| `static from_cstr(const char* cstr) noexcept -> Utf8StringRef` | Stable | 从 C 字符串构造视图，空指针返回空视图，O(n) 计算码点数 |
| `static from_data(const u8* data, usize byte_len, usize cp_len = npos) -> Utf8StringRef` | Stable | 从数据构造；`cp_len == npos` 时计算码点数；不复制 |

`from_data()` 的输入必须是有效 UTF-8。当前接口不抛错；如果输入非法，码点长度可能为 0，后续行为不再额外保证。

常量：

| 名称 | 行为 |
|---|---|
| `static constexpr usize npos = usize(-1)` | 查找未命中或默认未知码点长度 |

### 4.2 查询和访问

| 接口 | 复杂度 | 行为 |
|---|---|---|
| `length() const noexcept -> usize` | O(1) | 码点数量 |
| `byte_length() const noexcept -> usize` | O(1) | 字节数量 |
| `is_empty() const noexcept -> bool` | O(1) | 是否为空 |
| `data() const noexcept -> const u8*` | O(1) | 原始字节指针，不保证 `\0` 终止 |
| `byte_at(usize index) const -> u8` | O(1) | 无边界检查字节访问 |
| `code_point_at(usize index) const -> u32` | O(n) | 无边界检查码点访问；越界当前返回 0 |
| `to_std_string() const -> std::string` | O(n) | 复制为 `std::string` |

### 4.3 切片和子串

| 接口 | 行为 |
|---|---|
| `slice(usize byte_start, usize byte_end) const -> Utf8StringRef` | 返回字节区间 `[byte_start, byte_end)` 的视图；空区间或起点越界返回空视图；终点越界会截断 |
| `slice_by_cp(usize cp_start, usize cp_count) const -> Utf8StringRef` | 按码点切片；起点越界或数量为 0 返回空视图 |
| `substr(usize cp_start, usize cp_count) const -> Utf8String` | 按码点复制出拥有字符串 |

调用方应只在 UTF-8 码点边界上使用 `slice()`；传入码点中间字节会得到语义不完整的视图。

### 4.4 查找、拆分和变换

| 接口 | 行为 |
|---|---|
| `starts_with(const Utf8StringRef& prefix) const noexcept -> bool` | 字节级前缀判断 |
| `ends_with(const Utf8StringRef& suffix) const noexcept -> bool` | 字节级后缀判断 |
| `trim() const noexcept -> Utf8StringRef` | 去除首尾空白，返回视图 |
| `trim_start() const noexcept -> Utf8StringRef` | 去除开头空白，返回视图 |
| `trim_end() const noexcept -> Utf8StringRef` | 去除结尾空白，返回视图 |
| `split(const Utf8StringRef& delimiter) const -> std::vector<Utf8StringRef>` | 按分隔符拆分为视图；空字符串返回空列表；空分隔符返回自身 |
| `to_lower() const -> Utf8String` | 返回小写转换结果 |
| `to_upper() const -> Utf8String` | 返回大写转换结果 |
| `replace_all(const Utf8StringRef& from, const Utf8StringRef& to) const -> Utf8String` | 全局替换；`from` 为空时返回原内容复制 |
| `index_of(const Utf8StringRef& needle) const noexcept -> usize` | 返回首次出现的码点下标，未找到返回 `npos` |
| `index_of(const Utf8StringRef& needle, usize start_cp) const noexcept -> usize` | 从码点下标开始查找 |
| `index_of(u32 code_point) const noexcept -> usize` | 查找码点 |
| `index_of(u32 code_point, usize start_cp) const noexcept -> usize` | 从码点下标开始查找码点 |
| `contains(const Utf8StringRef& needle) const noexcept -> bool` | 是否包含子串 |

### 4.5 迭代和比较

| 接口 | 行为 |
|---|---|
| `begin() const noexcept -> Utf8Iterator` | 起始码点迭代器 |
| `end() const noexcept -> Utf8Iterator` | 结束迭代器 |
| `compare(const Utf8StringRef& other) const noexcept -> int` | 字节级字典序比较 |
| `equals(const Utf8StringRef& other) const noexcept -> bool` | 字节级相等 |
| `operator==` / `operator!=` | 等价于 `equals()` |

## 5. Utf8String

`Utf8String` 是拥有所有权、不可变、末尾 `\0` 终止的 UTF-8 字符串。

### 5.1 构造和所有权

| 接口 | 稳定性 | 行为 |
|---|---|---|
| `Utf8String() noexcept` | Stable | 空字符串，内部仍有 `\0` |
| `Utf8String(const u8* data, usize byte_length)` | Stable | 复制数据并校验 UTF-8；非法时抛 `std::runtime_error` |
| `explicit Utf8String(const char* cstr)` | Stable | 从 C 字符串复制；空指针构造空字符串 |
| `Utf8String(const Utf8String&) = delete` | Stable | 禁止隐式复制 |
| `Utf8String(Utf8String&&) noexcept` | Stable | 移动构造 |
| `~Utf8String()` | Stable | 释放内部数据 |
| `operator=(const Utf8String&) = delete` | Stable | 禁止隐式复制赋值 |
| `operator=(Utf8String&&) noexcept` | Stable | 移动赋值 |
| `clone() const -> Utf8String` | Stable | 显式深拷贝 |

工厂：

| 接口 | 稳定性 | 行为 |
|---|---|---|
| `static from_data(const u8* data, usize byte_len, usize cp_len = npos) -> Utf8String` | Stable | 复制数据；未传 `cp_len` 时校验并计数；传入时仍校验 UTF-8 |
| `static from_code_point(u32 cp) -> Utf8String` | Stable | 从单个码点构造；非法码点抛 `std::runtime_error` |
| `static from_cstr(const char* cstr) noexcept -> Utf8String` | Stable | 从 C 字符串构造；空指针或非法 UTF-8 返回空字符串 |

### 5.2 查询、访问和转换

| 接口 | 复杂度 | 行为 |
|---|---|---|
| `length() const noexcept -> usize` | O(1) | 码点数量 |
| `byte_length() const noexcept -> usize` | O(1) | 字节数量，不含结尾 `\0` |
| `is_empty() const noexcept -> bool` | O(1) | 是否为空 |
| `size() const noexcept -> usize` | O(1) | `length()` 的 STL 风格别名 |
| `empty() const noexcept -> bool` | O(1) | `is_empty()` 的 STL 风格别名 |
| `data() const noexcept -> const u8*` | O(1) | 原始字节指针，保证结尾 `\0` |
| `c_str() const noexcept -> const char*` | O(1) | C 字符串指针 |
| `byte_at(usize index) const -> u8` | O(1) | 无边界检查字节访问 |
| `code_point_at(usize index) const -> u32` | O(n) | 无边界检查码点访问；越界当前返回 0 |
| `to_std_string() const -> std::string` | O(n) | 复制为 `std::string` |

### 5.3 视图、切片和字符串操作

`Utf8String` 的以下接口语义与 `Utf8StringRef` 对应接口一致，区别是它们以当前字符串为数据源：

| 接口 |
|---|
| `ref() const noexcept -> Utf8StringRef` |
| `slice(usize byte_start, usize byte_end) const -> Utf8StringRef` |
| `slice_by_cp(usize cp_start, usize cp_count) const -> Utf8StringRef` |
| `begin() const noexcept -> Utf8Iterator` |
| `end() const noexcept -> Utf8Iterator` |
| `substr(usize cp_start, usize cp_count) const -> Utf8String` |
| `starts_with(const Utf8StringRef& prefix) const noexcept -> bool` |
| `ends_with(const Utf8StringRef& suffix) const noexcept -> bool` |
| `trim() const noexcept -> Utf8StringRef` |
| `trim_start() const noexcept -> Utf8StringRef` |
| `trim_end() const noexcept -> Utf8StringRef` |
| `split(const Utf8StringRef& delimiter) const -> std::vector<Utf8StringRef>` |
| `to_lower() const -> Utf8String` |
| `to_upper() const -> Utf8String` |
| `replace_all(const Utf8StringRef& from, const Utf8StringRef& to) const -> Utf8String` |
| `index_of(const Utf8StringRef& needle) const noexcept -> usize` |
| `index_of(const Utf8StringRef& needle, usize start_cp) const noexcept -> usize` |
| `index_of(u32 code_point) const noexcept -> usize` |
| `index_of(u32 code_point, usize start_cp) const noexcept -> usize` |
| `contains(const Utf8StringRef& needle) const noexcept -> bool` |

比较：

| 接口 | 行为 |
|---|---|
| `compare(const Utf8StringRef& other) const noexcept -> int` | 字节级字典序 |
| `compare(const Utf8String& other) const noexcept -> int` | 字节级字典序 |
| `equals(const Utf8StringRef& other) const noexcept -> bool` | 字节级相等 |
| `operator==(const Utf8String& other) const noexcept` | 相等 |
| `operator==(const Utf8StringRef& other) const noexcept` | 相等 |
| `operator!=` | 不相等 |

## 6. Utf8Iterator

`Utf8Iterator` 是前向迭代器，值类型为 `u32` 码点。

| 接口 | 行为 |
|---|---|
| `Utf8Iterator() noexcept` | 空迭代器 |
| `Utf8Iterator(const u8* pos, const u8* end) noexcept` | 从字节位置构造 |
| `operator*() const noexcept -> u32` | 解码当前码点 |
| `operator++() noexcept -> Utf8Iterator&` | 前进一个码点 |
| `operator++(int) noexcept -> Utf8Iterator` | 后置递增 |
| `operator==` / `operator!=` | 比较位置 |
| `byte_ptr() const noexcept -> const u8*` | 返回当前字节指针 |

下游可用范围 for：

```cpp
ca::str::Utf8String s("A中");
for (ca::u32 cp : s) {
    (void)cp;
}
```

## 7. 非成员接口

| 接口 | 行为 |
|---|---|
| `operator==(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept` | 对称比较 |
| `operator!=(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept` | 对称比较 |
| `split(const Utf8StringRef& str, const Utf8StringRef& delimiter) -> std::vector<Utf8StringRef>` | 调用 `str.split(delimiter)` |
| `join(const std::vector<Utf8StringRef>& parts, const Utf8StringRef& separator) -> Utf8String` | 用分隔符连接 |
| `operator<<(std::ostream& os, const Utf8StringRef& s) -> std::ostream&` | 按原始字节输出 |
| `operator<<(std::ostream& os, const Utf8String& s) -> std::ostream&` | 按原始字节输出 |
| `std::hash<Utf8String>` | 哈希支持 |
| `std::hash<Utf8StringRef>` | 哈希支持 |

哈希只承诺相同内容得到相同 hash，不承诺具体算法和数值长期不变。

## 8. Utf8StringArena

头文件：`libca/str/src/libca/str/utf8_string_arena.hpp`

`Utf8StringArena` 是追加式 UTF-8 字符串池。它复制输入字符串，去重后返回指向池内数据的 `Utf8StringRef`。

生命周期和线程规则：

- arena 析构后，所有由它返回的 `Utf8StringRef` 全部失效。
- `clear()` 后，之前返回的所有 `Utf8StringRef` 全部失效。
- 移动构造会转移内部 chunk，移动前已经返回的 ref 仍指向被转移后的存储；移动源对象不应继续依赖旧状态。
- 移动赋值会释放目标对象原有 chunk，因此目标对象旧 ref 会失效。
- 当前无锁，非线程安全。

### 8.1 构造和赋值

| 接口 | 行为 |
|---|---|
| `Utf8StringArena() noexcept` | 构造空池并分配初始 chunk |
| `~Utf8StringArena()` | 释放所有 chunk |
| `Utf8StringArena(const Utf8StringArena&) = delete` | 禁止复制 |
| `operator=(const Utf8StringArena&) = delete` | 禁止复制赋值 |
| `Utf8StringArena(Utf8StringArena&&) noexcept` | 移动构造 |
| `operator=(Utf8StringArena&&) noexcept` | 移动赋值 |

### 8.2 intern

| 接口 | 行为 |
|---|---|
| `intern(const u8* data, usize byte_length) -> Utf8StringRef` | 校验 UTF-8，复制到池内；空或非法输入返回空视图 |
| `intern(const char* cstr) -> Utf8StringRef` | 从 C 字符串 intern；空指针返回空视图 |
| `intern(const Utf8StringRef& str) -> Utf8StringRef` | intern 视图内容 |
| `intern(const Utf8String& str) -> Utf8StringRef` | intern 拥有字符串内容 |

去重规则：

- 内容相同的字符串返回同一份池内存储。
- 去重按 UTF-8 字节序列执行。
- 哈希冲突会继续比较完整字节内容。

### 8.3 统计和清理

| 接口 | 行为 |
|---|---|
| `size() const noexcept -> usize` | 唯一字符串数量 |
| `total_bytes() const noexcept -> usize` | 已分配 chunk 总容量，包含未使用空间 |
| `clear() noexcept` | 释放全部 chunk 和索引，重新回到空池状态 |

设计评价：arena 的批量生命周期模型明确，适合固定。文档必须强调 ref 失效条件和非线程安全。

## 9. 本次暂不冻结的声明

### 9.1 Utf8StringBuilder

当前可作为内部工具使用，但下游暂不应把它写入公共接口。原因是：

- 它暴露了可变缓冲和容量语义，未来可能被更完整的 builder 或 bytes 复用替代。
- `append(const u8*, usize)` 接受未校验字节，错误直到 `build()` 才体现。

### 9.2 ZUtf8StringRef

当前不建议固定，原因是：

- `from_std_string()` 返回的视图依赖传入 `std::string` 的生命周期。
- `from_static()` 的缓存按 `const char*` 地址作为 key，语义依赖字面量/全局常量地址稳定。
- 更适合作为后续单独设计文档固定。

## 10. 推荐用法

```cpp
#include <libca/str/utf8_string.hpp>
#include <libca/str/utf8_string_arena.hpp>

ca::str::Utf8String name("你好");
ca::str::Utf8StringRef first = name.slice_by_cp(0, 1);

ca::str::Utf8StringArena arena;
ca::str::Utf8StringRef key = arena.intern("config.name");

if (name.contains(first)) {
    auto copy = first.substr(0, first.length());
    (void)copy;
}
```

下游优先选择：

- 长期持有字符串：`Utf8String`。
- 临时参数和切片：`Utf8StringRef`。
- 批量解析、整体释放：`Utf8StringArena`。
- 需要 C API 交互：只对 `Utf8String` 使用 `c_str()`，不要对 `Utf8StringRef` 假设 `\0`。
