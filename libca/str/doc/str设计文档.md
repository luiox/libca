---
version: 1.3
update:
2026-08-09 - 新增格式化设施 format 章节（fmt 提升为 str public 依赖，提供 format/format_to/format_runtime 门面）
2026-07-20 - 新增代码页转换工具章节（CharsetConverter，从 libca.core 迁移并去 iconv）
2026-07-13 - 合并 str-spec.md 的设计性内容（所有权分层、Pool/Arena/Twine 选型与生命周期契约），删除逐接口清单
2026-07-06 - 首版，补充 str 模块职责、UTF-8 类型族与 StringUtil 字节工具边界
---

# libca::str 设计文档

> 本文讲 str 模块的架构、设计边界与选型。**具体接口签名与逐方法说明见各头文件的 Doxygen 注释**，
> 本文不重复 API 清单。涉及的头文件：
> `utf8_string.hpp`、`utf8_string_pool.hpp`、`utf8_string_arena.hpp`、`utf8_twine.hpp`、
> `utf16_string.hpp`、`cstring.hpp`、`wstring.hpp`、`conversion.hpp`、`charset.hpp`、`char_util.hpp`、
> `string_util.hpp`、`os_string.hpp`、`format.hpp`。

## 1. 模块定位

`libca::str` 提供字符串与文本处理基础设施。它位于 core 之上，只依赖 `libca_core`，为 fs、crypto、time 和上层业务提供统一的 UTF-8 与文本工具。

str 模块分成两类能力：

- **UTF-8 类型族**：`Utf8String`、`Utf8StringRef`、`ZUtf8StringRef`、builder、pool、arena、twine。
- **std::string 工具族**：`StringUtil`、编码转换、C/W string 适配、ASCII/URL-safe 文本工具。

## 2. UTF-8 类型族

UTF-8 是 libca 的内部文本编码。这一族类型围绕**所有权**组织，回答"这块 UTF-8 字节归谁管、活多久"：

```
拥有型（分配并持有字节）
  ├─ Utf8String        独立拥有，移动语义，clone() 为唯一深拷贝
  ├─ Utf8StringArena   整批共存共死：一批字符串有共同死亡点，整块释放
  └─ Utf8StringPool    条目寿命各异：引用计数，refcount 归零真删

非拥有视图（借用，不分配）
  ├─ Utf8StringRef     通用视图，不保证 \0（中段切片可能无 \0）
  ├─ ZUtf8StringRef    保证 \0 的视图，专为字面量/全局常量（from_static 带 intern 缓存）
  └─ Utf8StringPooledPtr  Pool 的引用计数句柄（8 字节），可降级为 Utf8StringRef 借用

惰性拼接（不持有，仅 materialize 时一次性产出）
  └─ Utf8Twine         LLVM Twine 风格二叉拼接树，拼接零分配

构建器（拥有可变缓冲，临时）
  └─ Utf8StringBuilder 追加写入，build() 校验并产出 Utf8String
```

### 2.1 拥有与不可变

- `Utf8String` 一旦构造完成内容不可变，所有"修改"操作（substr、大小写转换等）返回新实例。不可变意味着天然线程安全，且无需 COW、扩容等可变字符串的复杂度。
- 码点数量在构造时缓存，`length()` 为 O(1)；按码点访问为 O(n) 扫描，按字节访问为 O(1)。
- 深拷贝必须显式 `clone()`，拷贝构造/赋值已删除，避免意外深拷贝。
- 比较与查找按 UTF-8 字节字典序，不做 Unicode 规范化、不按 locale collation，结果稳定可预测。

### 2.2 视图与借用纪律

`Utf8StringRef` 是公共视图层，所有比较运算符最终都落在视图上。`Utf8String` 可隐式构造为视图参与比较。

视图不持有内存，**借用纪律由调用方保证**：原串销毁、移动，或 arena/pool 释放后，由它派生的视图失效。这是 Rust `&str` 的同款约束——视图是借用，不是所有权。

`Utf8StringRef` 不保证结尾 `\0`（因为可能是某串的中段切片），故**不提供 `c_str()`**。需要 `\0` 保证的场景用 `ZUtf8StringRef`（字面量/全局常量，经 `from_static` 走全局 intern 缓存避免重复算码点）或直接用 `Utf8String`（内部始终 `\0` 结尾）。

### 2.3 Arena vs Pool —— 两种池化策略

批量场景下逐个 `Utf8String` 分配开销大，提供两种池化去重方案，**按死亡点选型**：

| | Utf8StringArena | Utf8StringPool |
|---|---|---|
| 死亡模型 | 同生共死，整批释放 | 各条目寿命不同 |
| 句柄 | 返回 `Utf8StringRef`（借用 arena） | 返回 `Utf8StringPooledPtr`（引用计数） |
| 回收 | arena 析构/clear 时整块释放 | refcount 归零真删（非墓碑） |
| 典型场景 | 编译器符号表、配置解析 | 跨作用域共享的常量字符串 |

两者都做内容去重（intern），且**返回的视图/句柄绑定池的生命周期**：

- **Arena**：arena 析构 / `clear()` / 移动赋值后，所有派生 `Utf8StringRef` 失效。内存不挪动（固定块链式扩展），故指针稳定。
- **Pool 的生命周期契约（核心）**：
  - **真删**：`PooledPtr` refcount 归零即 delete 该 entry，不是墓碑标记。
  - **outlive 是软契约**：Pool 先于 PooledPtr 析构 / `clear()` / move-assign 时**不会 UAF**（走 disown fail-safe，存活句柄转为自管释放），但失去去重收益。遵守 outlive 则享真删+去重，违反则退化自管，仍安全。
  - **Ref 由借用纪律保证**：`PooledPtr` 析构后，由它 `.ref()` 派生的 `Utf8StringRef` 失效。

### 2.4 惰性拼接 Utf8Twine

`Utf8Twine` 是 LLVM Twine 风格的二叉拼接树，拼接（`operator+`/`concat`）零分配，只在真正需要时经 `to_string()`（独立拥有）/ `materialize(arena)`（intern 进 arena）/ `materialize(pool)`（intern 进 pool）一次性产出。

借用语义同 `Utf8StringRef`：**只在单表达式内作参数使用，绝不存成员、绝不跨语句**——子 Twine 按指针存，指向表达式内的栈临时量，跨语句即悬空。

```cpp
pool.intern(a + "/" + b);              // → PooledPtr，拼接零分配
arena.intern(Utf8Twine(x) + y);        // → Utf8StringRef
(a + b).to_string();                   // → Utf8String（独立拥有）
```

### 2.5 标准库互操作

UTF-8 字符串族可零拷贝接入标准库：`Utf8String` / `Utf8StringRef` / `ZUtf8StringRef` 均提供 `operator std::string_view()`（空后端安全回落，不触发 UB）。需要分配拷贝时用 `to_std_string()`。

## 3. StringUtil 的边界

`StringUtil` 面向 `std::string`，按字节或 ASCII 语义工作。它不承担完整 Unicode 分类、大小写折叠或规范化职责。

这种边界是有意的：

- 基础库很多场景只需要配置 key、URL 参数、日志字段、协议文本等轻量字节处理。
- 完整 Unicode 语义需要更大的数据表和规范支持，不应隐式塞进简单工具函数。
- UTF-8 码点语义应优先使用 `Utf8String` / `Utf8StringRef` 或 `char_util`。

## 4. URL-safe 文本工具

URL-safe 文本工具放在 `StringUtil` 中，因为它们处理的是 UTF-8 的字节表示，而不是 Unicode 码点语义。

当前提供三层能力：

- **ASCII 分类**：判断 ASCII 字母、数字、unreserved URL 字符，并做 ASCII 大小写转换。
- **Percent / form component**：按 RFC 3986 unreserved 集合保留字符，其它字节编码为 `%HH`；表单组件额外把空格编码为 `+`。
- **Base64url**：使用 `-` / `_` 替代 `+` / `/`，支持无 padding 和标准 padding。

decode 类接口返回 `Result<std::string, std::string>`，因为非法输入需要把错误原因交给调用方。Base64url 解码采用严格模式，会拒绝非法字符、非法 padding、非法长度和非零尾部填充位，避免同一字节串出现多个可接受编码。

## 5. 代码页转换工具（CharsetConverter）

`CharsetConverter`（`charset.hpp`）从旧 `libca.core/src/base/Charset.{hpp,cpp}` 迁移而来。
旧实现混用 Win32 `MultiByteToWideChar` + libiconv + C++17 弃用的 `<codecvt>`，存在多处
bug（iconv 失败判断用 `== 0` 而非 `==(iconv_t)-1`、固定 255 字节栈缓冲截断、`new wchar_t[0]`
不检查等），且依赖未 vendor 的 libiconv。迁移时统一为：

- **后端只保留 Win32 API**：`MultiByteToWideChar` / `WideCharToMultiByte`，去除 libiconv
  和 `<codecvt>` 依赖。GBK / GB2312 都通过 CP_936 处理（GBK 是 GB2312 超集，CP_936 在
  现代 Windows 上等价于 GBK）；本地代码页用 CP_ACP。
- **错误模型改为 `ca::core::Status` / `StatusResult<T>`**：非法 UTF-8 / UTF-16 返回
  `INVALID_ARGUMENT`，Win32 调用失败返回 `INTERNAL` 并附 `GetLastError()`。
- **跨平台头文件**：非 Windows 平台所有方法返回 `UNIMPLEMENTED`，但头文件可被任意平台
  引用，便于跨平台代码引用同一签名走错误分支。
- **删除未实现的桩**：旧 `mstrToU16str` / `mstrToU32str` / `Charset::encode` 等声明但未定义
  的方法直接删除；UTF-8 ↔ UTF-16 raw 转换由已有 `conversion.hpp` 的 `utf8_to_utf16` 等提供。

GBK 等中文遗留码页是 Windows 概念，新库不打算为它引入跨平台依赖。如果未来确有跨平台
中文转码需求，应基于 ICU 或平台原生 API 单独设计，而不是复活 libiconv 路径。

## 6. 依赖与错误模型

str 只依赖 core。需要错误传播的轻量文本工具使用 `Result<T, std::string>`；领域错误枚举暂不引入，避免为很小的解析错误建立过重类型。

公共头文件写使用说明，`.cpp` 只保留实现关键点注释。设计文档不重复 API 清单，只解释为什么这样分层。

## 7. 测试策略

测试位于 `libca/str/unittest/`，按组件拆分。重点覆盖：

- UTF-8 合法/非法输入。
- 字节长度与码点长度一致性。
- 所有权、移动、池化和 arena 生命周期。
- ASCII/URL-safe 工具的 roundtrip 与非法输入拒绝。

URL-safe 工具尤其要覆盖错误路径，因为解析类函数一旦放宽非法输入，后续协议和缓存 key 很容易出现不可见的不一致。

## 8. OsString —— 平台原生字符串

`OsString` / `OsStr` 对齐 Rust `std::ffi::OsString` / `OsStr`，承载平台**原生编码**的字符串：

- Windows：内部存 UTF-16（`std::wstring`），与 Win32 `*W` API 零拷贝互转。
- POSIX：内部存 UTF-8（`Utf8String`），与文件系统零拷贝互转。

### 设计动机

UTF-8 是 libca 的统一交换编码，但调用 Windows API（`CreateFileW`/`GetEnvironmentVariableW` 等）
时需要 UTF-16；分散手写转换易遗漏错误处理。OsString 把"平台原生"语义显式化，集中处理编码。

### 关键约束

- **不做隐式 UTF-8 转换**：与 Utf8String 互转必须显式调用，名字标注编码开销
  （`to_utf8_lossy` / `from_utf8`），避免无意中触发 O(n) 编码转换。
- **move-only**：POSIX 持有 move-only 的 Utf8String，因此 OsString 整体 move-only，
  与 Rust OsString 语义一致。
- **零拷贝平台互操作**：`as_wide()`（Windows）/ `as_utf8()`（POSIX）返回内部存储视图；
  `into_wstring()` / `into_utf8_string()` move 出所有权，均无拷贝。

### 与 fs / env 的关系

`libca.fs` 当前以 UTF-8 `std::string` + `std::filesystem::u8path` 承载路径；OsString 提供
了平台原生载体，后续可按需为其增加 PathUtil 重载。`libca.env` 在 Windows 上直接用 Win32
API 做 UTF-8↔UTF-16，暂未依赖 OsString（保持系统层不跨层依赖 str）。

## 9. 格式化设施 format

`format.hpp` 提供基于 fmt 的 `{}`-style 格式化门面，对标 Rust `format!` / `format_args!`。
fmt 是 C++20 `std::format` 的原型，API 几乎一致；用 fmt 等于在 C++17 上提前拿到标准设施，
将来升 C++20 可平滑切到 `std::format`。

### 9.1 为什么 fmt 挂在 str

fmt 最初由 `libca.log`（L3）引入作为日志门面的格式化后端。但 fmt 是"基础设施级"的库——
opt/env/io/http 等模块都有格式化需求（错误消息拼接、版本号、协议字段）。若 fmt 只挂在 log，
其它模块要用就得跨层依赖 log，破坏 `spec` 的依赖分层（log 在 L3，不能被 L2/L3 同层模块依赖）。

因此把 fmt 提升为 str 的 **public 依赖**：fmt 在 str（L1）声明，通过 `add_deps("libca_str")`
向下游传递 include path。这样：

- fmt 全库单一来源在 str/L1，分层干净（任何模块都可依赖 str）。
- log 改为依赖 str 间接拿 fmt，删掉自己的 `add_requires("fmt")`。
- 门面 `ca::str::format` 建立在 fmt 之上，对内封装第三方库（便于将来切 `std::format`），
  对外提供围绕 Utf8String 类型族的一致接口。

### 9.2 门面 API 与 UTF-8 校验契约

| API | 行为 | 对标 |
|---|---|---|
| `format(fmt, args...)` → `Utf8String` | 编译期校验格式串，返回拥有型、已校验 UTF-8 | Rust `format!` |
| `format_to(Utf8StringBuilder&, fmt, args...)` | 追加到 builder，build() 时统一校验 | `format_args!` + write |
| `format_to(std::string&, fmt, args...)` | 追加到 std::string，不校验 UTF-8 | —— |
| `format_runtime(fmt_str, args)` → `Utf8String` | 运行期格式串（无编译期校验） | `format!` 运行期等价 |

**UTF-8 校验**：`format` / `format_runtime` 走 `Utf8String(const u8*, usize)` 构造，参数产出
非法字节时抛 `std::runtime_error`，与 Utf8String 既有契约一致。`format_to(Utf8StringBuilder)`
在最终 `build()` 时校验。`format_to(std::string)` 不校验，给日志后端/协议代码等字节级场景。

### 9.3 formatter 特化

fmt 12.x 不再自动识别 `operator std::string_view()` 的隐式转换，故 `format.hpp` 显式特化了
`fmt::formatter<Utf8String>` / `fmt::formatter<Utf8StringRef>`，转发到 string_view 的 formatter
（零拷贝）。特化放在 `namespace fmt`——这是标准库/第三方类型特化的合法位置。

### 9.4 命名取舍：format_runtime 而非 vformat

fmt 自身导出同签名的 `fmt::vformat(fmt::string_view, fmt::format_args)`。若本门面也叫
`vformat`，用户 `using namespace ca::str` 后调用会与 `fmt::vformat` 经 ADL 产生二义。
故运行期版本命名为 `format_runtime`，语义更清晰（运行期格式串）且避开冲突。

### 9.5 与 log 的关系

log 的 `OpaqueFormat` + `FmtArgsHolder`（Rust `format_args!` 的 C++ 翻版，view 语义、延迟渲染）
设计保留不动，后端仍基于 `render_to(std::string&)`。本次只迁移 fmt 的依赖来源（从 log 私有
改为 str public），不改 log 的格式化架构。日志内部若要 Utf8String 化，后续单独演进。


