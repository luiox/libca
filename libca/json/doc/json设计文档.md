---
version: 1.0
update:
2026-07-19 - 首版，说明 JSON 模块的 SAX/DOM 双形态、ca::str 集成与错误模型
---

# libca/json 设计文档

## 定位

`libca_json` 是 JSON 文本读写模块，遵循 RFC 8259，同时提供可选的非标准宽松扩展（尾随逗号、
注释）。它同时支持两种使用形态：

- **SAX（事件流）**：用户实现 `JsonHandler`，由 `JsonParser` 驱动。适合流式处理大文件、
  按需取值、零内存峰值。
- **DOM（树）**：`JsonReader::read()` 一次性构造 `JsonValue` 树，适合随机访问和编辑。

模块命名空间是 `ca::json`，构建目标是 `libca_json`，单元测试目标是 `libca_json_unittest`。
依赖 `libca_core` 和 `libca_str`，不依赖其它 libca 模块。

## 与 ca::str 的集成

本模块**深度集成 `ca::str`**，覆盖 `spec/cpp-code-spec.md` 中 "ca::str 定稿前新模块默认用
std::string" 的临时约定。具体集成点：

- 输入文本用 `Utf8StringRef`（非拥有视图，零拷贝指向调用方持有的原文本）。
- DOM 字符串值（string 节点、object key）用 `Utf8String`（拥有所有权）。
- 解析过程中的字符串累积用 `Utf8StringBuilder`（转义解码、码点拼接）。
- 错误消息用 `Utf8String`。

这一选择基于"库内字符串原语统一"的诉求：用户已有的 `ca::str` 字符串无需来回和
`std::string` 互转。代价是 `ParseError`（含 `Utf8String`）成为 move-only 类型——`Result` 在
`ca::Err(...)` 构造时移动而非拷贝。

## 架构

模块分为五层，自底向上：

- `SourceLocation` / `ParseError`：位置与错误（字节偏移 + 1-based 行/列 + 消息）。
- `JsonValue`：DOM 数据模型，七种类型。
- `JsonHandler`：SAX 事件接口（虚基类，默认空实现，`on_error` 纯虚）。
- `JsonParser`：递归下降解析器，词法与语法分析合一，驱动 `JsonHandler`。
- `JsonDomBuilder`：`JsonHandler` 的 DOM 装配实现；`JsonReader` = `JsonParser` +
  `JsonDomBuilder`。
- `JsonWriter`：把 `JsonValue` 序列化为 `Utf8String`。

关键设计：**DOM 建立在 SAX 之上**。`JsonReader::read()` 内部构造一个 `JsonDomBuilder`，
把它喂给 `JsonParser`，解析器吐事件、builder 把事件装配成 `JsonValue` 树。这样 SAX 解析器
是唯一的核心，DOM 只是它的一个消费者——代码量约等于单一风格解析器，而非两套独立实现。

## JsonValue 的类型设计

七种类型用 `JsonType` 枚举 + `std::variant` 存储：

| 类型     | variant 替代项            |
|----------|---------------------------|
| Null     | `std::monostate`          |
| Bool     | `bool`                    |
| Int      | `ca::i64`                 |
| Float    | `ca::f64`                 |
| String   | `ca::str::Utf8String`     |
| Array    | `std::vector<JsonValue>`  |
| Object   | `std::vector<ObjectMember>`，`ObjectMember = std::pair<Utf8String, JsonValue>` |

Object 用 `std::pair` 而非自定义结构体，是为了打破 "JsonValue 内含 JsonValue" 的循环定义：
`std::vector` 支持不完整元素类型，`std::pair` 由标准库完整定义。Object 成员的 `first` 是
key，`second` 是 value。

Object **保序**（按插入顺序），`set()` 覆盖同名 key（不重排）。

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

## 与 ini / csv 的关系

本模块是 libca"多格式读写库"系列的第一份（json），确立了后续格式库（ini 重写、csv 风格统一）
的样板：

- 四件套形态：`Document`（数据模型）+ `Reader`/`Parser`（解析）+ `Writer`（序列化）+
  `Options` 结构。
- 输入 `Utf8StringRef`，DOM 值 `Utf8String`，错误 `ParseError{SourceLocation, ...}`。
- 命名/分层/构建脚本风格与 `libca_csv`、`libca_ini` 一致。

`SourceLocation` 和 `ParseError` 当前定义在 json 模块内部。等第二个格式库（ini 重写）完成后，
如果出现明显的重复，再回抽为独立的公共层（暂不预先抽，避免过度设计）。

## 性能权衡

本模块**不追求 simdjson 级性能**，优先正确性、可读性和与 ca::str 生态的一致性：

- 递归下降而非 simd 向量化。
- DOM 用 `std::variant` + `std::vector`，不做 arena 池化或 key 去重。
- 文件读取走 `std::ifstream`（与 csv/ini 一致），不引入 fs 依赖。

如果未来出现性能瓶颈，可在不破坏公开 API 的前提下替换内部实现（如 arena key 池、
零拷贝字符串视图节点等）。
