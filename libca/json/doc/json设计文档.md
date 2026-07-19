---
version: 1.1
update:
2026-07-19 - 首版，说明 JSON 模块的 SAX/DOM 双形态、ca::str 集成与错误模型
2026-07-19 - v1.1：迁移到 Arena 架构，引入 JsonDocument，SAX/DOM 字符串统一 Utf8StringRef
---

# libca/json 设计文档

## 定位

`libca_json` 是 JSON 文本读写模块，遵循 RFC 8259，同时提供可选的非标准宽松扩展（尾随逗号、
注释）。它同时支持两种使用形态：

- **SAX（事件流）**：用户实现 `JsonHandler`，由 `JsonParser` 驱动。适合流式处理大文件、
  按需取值。
- **DOM（树）**：`JsonReader::read()` 一次性构造 `JsonDocument`（含 arena + root JsonValue），
  适合随机访问和编辑。

模块命名空间是 `ca::json`，构建目标是 `libca_json`，单元测试目标是 `libca_json_unittest`。
依赖 `libca_core` 和 `libca_str`，不依赖其它 libca 模块。

## 与 ca::str 的集成（Arena 架构）

本模块**深度集成 `ca::str`**。**v1.1 起采用 Arena 架构**（与 toml 模块统一）：

- 输入文本用 `Utf8StringRef`（非拥有视图，零拷贝指向调用方持有的原文本）。
- DOM 字符串值（string 节点、object key）用 `Utf8StringRef`（**非拥有**视图，指向所属
  `JsonDocument` 内的 `Utf8StringArena`）。
- SAX 字符串事件（`on_string` / `on_object_key`）同样传 `Utf8StringRef`，指向 parser 构造时
  传入的 arena（DOM 路径下即 `JsonDocument::arena()`，纯 SAX 路径下由用户自管）。
- 解析过程中的字符串累积用 `Utf8StringBuilder`（转义解码、码点拼接），完成后 `arena.intern(...)`
  入池。
- 错误消息用 `Utf8String`（拥有所有权——错误要走出 document 生命期）。

**核心动机**：消灭零散内存分配。旧版 DOM 用 `Utf8String`（每处独立堆分配、move-only），
改为 Arena 后所有字符串字节进 `Utf8StringArena`（64KB/chunk 链式），析构 = arena 释放几个
chunk，零散分配清零。附带 bonus：`intern` 自动去重，object key 天然去重省内存。

**所有权根**：`JsonDocument` 持有 `Utf8StringArena arena_` + `JsonValue root_`，禁拷贝、
仅移动。`JsonReader::read()` 返回 `Result<JsonDocument, ParseError>`——document 把 arena + tree
一起 move 给用户。

代价是 `ParseError`（含 owning `Utf8String`）成为 move-only 类型——`Result` 在 `ca::Err(...)`
构造时移动而非拷贝；`JsonReader::read()` 内部用 `clone_error()`（`message.clone()`）从
`const ParseError&` 重建可被 Result move 的副本。

## 架构

模块分为六层，自底向上：

- `SourceLocation` / `ParseError`：位置与错误（字节偏移 + 1-based 行/列 + 消息）。
- `JsonValue`：DOM 数据模型，七种类型。字符串与 object key 均为 `Utf8StringRef`。
- `JsonDocument`：所有权根，持有 `Utf8StringArena` + `JsonValue root_`，禁拷贝、仅移动。
- `JsonHandler`：SAX 事件接口（虚基类，默认空实现，`on_error` 纯虚）。
- `JsonParser`：递归下降解析器，词法与语法分析合一，驱动 `JsonHandler`；构造时接收 arena
  引用，所有字符串经 `arena.intern(...)` 入池。
- `JsonDomBuilder`：`JsonHandler` 的 DOM 装配实现；`JsonReader` = `JsonParser` +
  `JsonDomBuilder`。
- `JsonWriter`：把 `JsonDocument` 序列化为 `Utf8String`。

关键设计：**DOM 建立在 SAX 之上**。`JsonReader::read()` 内部构造一个 `JsonDocument` +
`JsonDomBuilder`，把 builder 喂给 `JsonParser`（同时传入 document 的 arena），解析器吐事件、
builder 把事件装配成 `JsonValue` 树，最后写入 `document.root()`。SAX 解析器是唯一的核心，
DOM 只是它的一个消费者——代码量约等于单一风格解析器，而非两套独立实现。

## JsonValue 的类型设计

七种类型用 `JsonType` 枚举 + `std::variant` 存储：

| 类型     | variant 替代项                |
|----------|-------------------------------|
| Null     | `std::monostate`              |
| Bool     | `bool`                        |
| Int      | `ca::i64`                     |
| Float    | `ca::f64`                     |
| String   | `ca::str::Utf8StringRef`      |
| Array    | `std::vector<JsonValue>`      |
| Object   | `std::vector<ObjectMember>`，`ObjectMember = std::pair<Utf8StringRef, JsonValue>` |

Object 用 `std::pair` 而非自定义结构体，是为了打破 "JsonValue 内含 JsonValue" 的循环定义：
`std::vector` 支持不完整元素类型，`std::pair` 由标准库完整定义。Object 成员的 `first` 是
key，`second` 是 value。

Object **保序**（按插入顺序），`set()` 覆盖同名 key（不重排）。

### 与旧版的关键差异（move-only 解除）

旧版 String 用 `Utf8String`（move-only），`JsonValue` 因此 `= delete` 拷贝。改用
`Utf8StringRef` 后，`JsonValue(const JsonValue&) = default`——整体可拷贝（浅拷贝引用），
`builder`/`push_back`/`emplace` 都更简单。`clone()` 仍保留，但语义变为"结构深拷贝 + 字符串
引用共享原 arena"（字符串引用不复制 arena）。需要完全独立的副本时另起 `JsonDocument`。

### 生命周期约束

`JsonValue` 与其字符串引用的生命周期绑定到所属 `JsonDocument`：

```cpp
const auto& host = doc.root().find("server")->find("host")->as_string();
// host 是 Utf8StringRef，指向 doc 内部 arena。
// doc 析构/clear/move-assign 后 host 失效——用户不能把 host 存到比 doc 更长的地方。
// 需要长期持有？拷出 Utf8String：
ca::str::Utf8String owned(host.data(), host.byte_length());
```

### number 的 int/float 判定

JSON 规范不区分整数与浮点。本库按字面量形态判定：不含 `.`/`e`/`E` 的数字解析为 Int（i64），
否则为 Float（f64）。超出 i64 范围的整数（如 `9223372036854775808`）自动降级为 Float。
访问用 `as_int()` / `as_float()`（类型不符断言）或 `as_int_or(fallback)` /
`as_float_or(fallback)`（安全互转：Int→Float 自动，Float→Int 截断）。

## 解析策略

`JsonParser` 是递归下降 + 词法合一的单趟扫描器。要点：

- **词法不分独立类**：JSON token 几乎单字符可定，`peek()` / `advance()` 直接驱动。
- **位置追踪**：`advance()` 同步更新 `SourceLocation`（行遇 `\n` 自增、列遇 `\n` 重置）。
  列按字节计（UTF-8 多字节算多列），文档与实现一致。
- **空白**：识别 `space`/`\t`/`\n`/`\r`。注释仅在 `allow_comments` 时识别 `//` 行注释和
  `/* */` 块注释（块注释未闭合报错）。
- **字符串**：完整支持 `\" \\ \/ \b \f \n \r \t \uXXXX`，含 UTF-16 surrogate pair 拼接
  （`\uD83D\uDE00` → U+1F600）。孤立高代理、未配对低代理、非法转义、未转义控制字符
  （U+0000–U+001F）均报错带位置。
- **number**：手写字符级扫描（不用 `strtod`/`strtoll` 直接吃整个输入，否则丢位置信息），
  判别 Int/Float 后用 `strtoll`/`strtod` 转换，`errno == ERANGE` 时 Int 降级 Float 或报错。
  前导零（`01`）、`-` 后无数字、`.` 前后缺数字均报错。
- **深度限制**：默认最大嵌套 1000 层（防栈溢出），超出报 `"nesting too deep"`。
- **BOM**：拒绝（报错），不静默吞。
- **错误恢复**：首错即停，不收集多错误（避免过度设计）。

## 写出策略

`JsonWriter` 把 `JsonValue` 序列化为符合 RFC 8259 的 JSON：

- 字符串转义：`"` `\\` 控制字符必转义为 `\"` `\\` `\b` `\f` `\n` `\r` `\t` 或 `\u00XX`。
- 非 ASCII 字符默认原样输出（已是合法 UTF-8 字节）。开启 `ensure_ascii` 后转义为
  `\uXXXX`（BMP）或 surrogate pair（astral plane，U+10000 以上）。
- 浮点用 `%.17g` 格式化，保证 double round-trip 精度。
- 默认紧凑输出；`pretty` 开启后按 `indent` 空格数换行缩进。空数组 `[]` 和空对象 `{}`
  即使在 pretty 模式也保持紧凑。

## 错误模型

- 解析错误用 `ParseError{SourceLocation, Utf8String}`，包含字节偏移、1-based 行/列、人读消息。
- 不用异常，全程经 `Result<T, ParseError>` 传播。
- `JsonParser::parse()` 返回 `bool`（成功/失败），首个错误经 `handler.on_error()` 报告，
  并由 `parser.last_error()` 取得——这种设计避免了 `Result<void, ParseError>` 在 move-only
  `ParseError` 上需要拷贝的尴尬。
- `JsonReader::read()` 把 `parser.last_error()` 经 clone 后包进 `Result` 返回。
- `JsonWriter::write()` 的字符串序列化不失败；`write_file()` 用 `Result<void, Utf8String>`
  表达打开/写入失败。

## 与 ini / csv / toml 的关系

本模块是 libca"多格式读写库"系列的第一份（json），确立了后续格式库的样板：

- 四件套形态：`Document`（数据模型）+ `Reader`/`Parser`（解析）+ `Writer`（序列化）+
  `Options` 结构。
- 输入 `Utf8StringRef`，错误 `ParseError{SourceLocation, ...}`。
- 命名/分层/构建脚本风格与 `libca_csv`、`libca_ini`、`libca_toml` 一致。

**Arena 架构与 toml 统一**：本模块（v1.1）已迁移到与 `libca_toml` 一致的 Arena 架构
（`Document` 持 arena + root，DOM 字符串用 `Utf8StringRef`）。`libca_ini` 和 `libca_csv`
也已迁移到 Arena（csv 用 `intern_raw` 以兼容字节级格式）。

`SourceLocation` 和 `ParseError` 当前定义在各模块内部。等所有格式库都迁完 Arena，
如果出现明显的重复，再回抽为独立的公共层 `libca/textio`（暂不预先抽，避免过度设计）。

## 性能权衡

本模块**不追求 simdjson 级性能**，优先正确性、可读性和与 ca::str 生态的一致性：

- 递归下降而非 simd 向量化。
- DOM 用 `std::variant` + `std::vector`；字符串统一经 `Utf8StringArena` 入池（去重 + 集中释放）。
- 文件读取走 `std::ifstream`（与 csv/ini/toml 一致），不引入 fs 依赖。
