---
version: 1.0
update:
2026-07-19 - 首版，说明 TOML 模块的 Arena 架构、DOM 数据模型、解析器实现要点
---

# libca/toml 设计文档

## 定位

`libca_toml` 是 TOML 1.0 文本读写模块（DOM 形态），是 libca 多格式读写库系列的第四份
（前三份为 json / ini / csv）。模块命名空间是 `ca::toml`，构建目标是 `libca_toml`，
单元测试目标是 `libca_toml_unittest`。依赖 `libca_core` 和 `libca_str`，不依赖其它 libca
模块。

主要使用场景是**配置文件**：替换 JSON 写的配置（JSON 不适合手写修改，且 nlohmann_json
依赖拉取麻烦、`std::string` 与 `ca::str` 来回转换繁琐）。TOML 正是为手写配置设计。

## 为什么本轮改用 Arena 架构（与 json/ini 的核心差异）

已合入的 json/ini 在 DOM 内部用 `ca::str::Utf8String`（每处独立堆分配、move-only）。
**本 TOML 模块改用 Arena 架构**：

- DOM 节点存 `Utf8StringRef`（16 字节、可拷贝、无独立堆分配）。
- 所有字符串字节进 `ca::str::Utf8StringArena`（连续大块 64KB/chunk 链式）。
- 析构 = arena 释放几个 chunk，零散分配清零。
- **附带 bonus**：`intern` 自动去重，Table 的 key 天然去重省内存。

**核心动机**（用户原话）："内存申请和释放有问题，用 Arena 直接消灭这些零散的所有内存分配，
只需要整个 dom 销毁时候整个字符串 arena 销毁就行。"

**Arena 方案先在 TOML 验证**（新代码无包袱）。TOML 合入并稳定后，再考虑把 json/ini
迁移到 Arena（另开 PR，复用 TOML 验证过的模式）。本轮不动 json/ini。

## 所有权模型：TomlDocument

核心问题：DOM 节点存 `Utf8StringRef` 后，节点不拥有字符串了，谁拥有 arena？

答案：引入 `TomlDocument` 根容器，内部持有 arena + root value。用户持有 document，
document 析构时 arena 一起释放。

```cpp
class TomlDocument {
public:
    TomlDocument();
    ~TomlDocument();  // 释放 arena（所有 ref 失效）
    TomlDocument(TomlDocument&&) noexcept;
    TomlDocument& operator=(TomlDocument&&) noexcept;
    // 禁拷贝（含 arena，不能共享）
    TomlValue& root() noexcept;
    const TomlValue& root() const noexcept;
    ca::str::Utf8StringArena& arena() noexcept;
private:
    ca::str::Utf8StringArena arena_;
    TomlValue root_;  // 默认 Table
};
```

`TomlReader::read()` 返回 `Result<TomlDocument, ParseError>`——document 把 arena + tree
一起 move 给用户。

### 生命周期约束

```cpp
const auto& host = doc.root().find("server")->find("host")->as_string();
// host 是 Utf8StringRef，指向 doc 内部 arena。
// doc 析构后 host 失效——用户不能把 host 存到比 doc 更长的地方。
// 需要长期持有？拷出 Utf8String：
ca::str::Utf8String owned(host.data(), host.byte_length());
```

## TomlValue：DOM 节点存 Utf8StringRef

10 种类型 + `std::variant` 存储：

| TOML 类型 | DOM `TomlType` | variant 成员 |
|-----------|----------------|--------------|
| String | `String` | `Utf8StringRef` |
| Integer | `Integer` | `ca::i64` |
| Float | `Float` | `ca::f64` |
| Boolean | `Boolean` | `bool` |
| Offset date-time | `OffsetDatetime` | `TomlDatetime` |
| Local date-time | `LocalDateTime` | `TomlDatetime` |
| Local date | `LocalDate` | `TomlDatetime` |
| Local time | `LocalTime` | `TomlDatetime` |
| Array | `Array` | `vector<TomlValue>` |
| Table | `Table` | `vector<pair<Utf8StringRef, TomlValue>>` |

### 关键变化（对比 json::JsonValue）

- string 存 `Utf8StringRef` 而非 `Utf8String` → TomlValue **可拷贝**（Utf8StringRef 可拷贝），
  move-only 约束解除。这大大简化 builder/parser 逻辑（`push_back`、`emplace` 都简单了），
  也让用户拷贝子树开销很低（浅拷贝引用）。
- Table 的 key 也是 `Utf8StringRef`（去重后入池）。
- `as_string()` 返回 `const Utf8StringRef&`——用户不能长期持有（arena 销毁后失效），
  文档强约束。

### TomlDatetime：4 种变体共用结构

```cpp
struct TomlDatetime {
    TomlDatetimeKind kind;  // OffsetDatetime / LocalDateTime / LocalDate / LocalTime
    ca::i32 year, ca::u8 month, day;     // 日期分量
    ca::u8 hour, minute, second;          // 时间分量
    ca::u32 nanos;                        // 小数秒纳秒（0-999999999）
    bool has_tz; ca::i16 tz_minutes;      // 时区（仅 OffsetDatetime）
};
```

用一个结构 + Kind 枚举区分，而非 4 个独立 variant 分支——减少类型分支数。

## 解析器实现要点

`TomlParser` 是单趟递归下降 + 行扫描。TOML 是面向行的格式：每行要么是 table header
`[a.b]` / `[[a.b]]`，要么是 key = value 行，要么是空行/注释。

### 值字面量扫描的特殊处理

TOML 值字面量的有效字符：数字、字母、+、-、.、:、T、Z、_。扫描时遇到空白/换行/注释/
`#`/`=`/`]`/`}`/`,` 即认为字面量结束。**例外**：date-time 字面量允许在日期与时间之间
用单个空格替代 T（如 `1979-05-27 07:32:00`），扫描器在识别到日期前缀时吃下这一个空格。

### Table 组织（最易出 bug 的部分）

三种声明各有不同语义：

- **标准表 `[a.b.c]`**：定义从根开始的完整路径，自动创建中间表。重复定义已存在的表头
  （或其父表已被前一个表头定义过）报错。
- **内联表 `k = { x = 1 }`**：单行，DOM 表节点，不参与 [header] 重复检测的"已被定义"
  语义（但解析期内 key 冲突仍报错）。
- **数组表 `[[products]]`**：每次出现追加一个新 table 到 array 末尾；子表 `[x.sub]` 归属
  **最近定义的 `[[x]]` 元素**。

### 重复检测策略

用一个扁平的 `std::vector<Utf8String>` 存已显式 [header] 定义的路径串（用 `\x1F` 分段）。
遇到新 [header] 时检查：

- 完全相同 → 重复定义，报错。
- 新路径是某个已定义路径的前缀 → 父表已被子表隐式创建，不能再 [header] 命名，报错。
  （如 `[a.b]` 后再 `[a]`。）

数组表 `[[x]]` 不参与"重复定义"判断——每次都追加新元素。

### 数值解析

整数按进制前缀分流（`0x`/`0o`/`0b` → `strtoll` 对应基数；十进制 → `strtoll` 基数 10）。
下划线分隔先剥离再调用 `strtoll`。**整数超 i64 范围报错**（不降级 float，与 TOML 严格
语义一致）。浮点用 `strtod`，特殊值 `inf`/`nan` 单独识别（大小写不敏感、可带符号）。

**不用 `std::from_chars`**：mingw 工具链对 float 的 from_chars 支持不全，与 json 一致
用 `strtoll` / `strtod`（带 errno 检查）。

### 错误处理

- **首错即停**，不收集多错误（与 json/ini 一致）。
- 错误带行号（列号也维护，但 TOML 行级错误定位足够）。
- 重复 key、重复 table header、整数溢出 → 全部报错。

## Writer 实现要点

`TomlWriter::write` 把 `TomlDocument` 序列化为 TOML 文本：

- 顶层 key=value 行在前，子表（含数组表）以 `[a.b]` / `[[a.b]]` 表头分段在后。
- 字符串默认用 basic string 加必要转义；含换行时用 multiline basic string。
- datetime 按 RFC 3339/ISO 8601 格式输出（含 `Z` 或 `±HH:MM` 时区）。
- 浮点整数形态（如 `1.0`）输出时追加 `.0`，避免 reparse 时被当整数。
- `inf`/`nan`/`-inf` 直接输出字面量。

### key 序列化

bare key（`A-Za-z0-9_-` 且非空）原样输出；否则用 basic string 包裹。

## 与 json/ini 的关系

- **本 PR 不动 json/ini**。它们仍用 Utf8String（move-only DOM）。
- TOML 用 Arena + Utf8StringRef 作为**新架构的验证用例**。
- TOML 合入稳定后，再开 PR 把 json/ini 迁移到 Arena（复用 TOML 的 TomlDocument 模式：
  引入 `JsonDocument` 持有 arena + root）。

## 已知坑（实现期间踩到的）

1. **`Utf8StringRef` 在 `libca/str/utf8_string.hpp`**，每个需要它的头 `#include
   "libca/str/utf8_string.hpp"`，没有独立的 `utf8_string_ref.hpp`。
2. **`Utf8String` 不可拷贝（move-only）**。Result 取错误用 `std::move(result).unwrap_err()`。
3. **variant 内含 `vector<TomlValue>` / `vector<pair<...>>` 的循环依赖**：用 `std::pair`
   做 table 成员（pair 由标准库完整定义），不要用自定义 struct。
4. **不要用 `std::from_chars`**：mingw 工具链对 float 支持不全。用 `strtoll`/`strtod`。
5. **`Utf8StringBuilder::append` 没有 `(const char*, usize)` 重载**（会和 `(const u8*, usize)`
   冲突）。需要按长度追加字节时用 `append(reinterpret_cast<const u8*>(s), len)`。
6. **MSVC 工具链**：`xmake f -p windows -a x64 -y --with_tests=y --with_em=n`。
   不指定平台会回落 mingw，gtest 编译失败。
7. **行尾注释不能以 `\` 结尾**：`advance();  // \` 会触发 MSVC 的 C4010 警告（行继续符）。
