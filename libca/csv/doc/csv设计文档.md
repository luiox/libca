---
version: 1.3
update:
2026-07-19 - v1.3 迁移到 Arena 架构：CsvDocument 内嵌 Utf8StringArena，字段经 intern_raw
             入池存 Utf8StringRef（不校验 UTF-8，保留任意字节）
2026-07-19 - v1.2 接入 ca::str：Reader 输入改 Utf8StringRef、Writer 输出改 Utf8String、
             错误改 ParseError（带行+列）。数据模型内部仍用 std::string（CSV 不规定编码）
2026-07-14 - 删除冗余英文摘要 design.md 与使用文档（并入 README），本文成为 csv 唯一设计文档
2026-07-06 - 首版，说明 CSV 独立模块的数据模型、Reader/Writer 分层与格式策略
---

# libca/csv 设计文档

## 定位

`libca_csv` 是独立的 CSV 文本读写模块，替代旧 `utility` 中的 `CsvFile`。它只负责
CSV 表格数据和文本格式之间的转换，不绑定业务 schema。

模块命名空间是 `ca::csv`，构建目标是 `libca_csv`，单元测试目标是
`libca_csv_unittest`。依赖 `libca_core` 和 `libca_str`。

## 架构

模块分为三层：

- `CsvDocument` / `CsvRow`：数据模型层（Arena 架构），保存可选标题行和记录行。
- `CsvReader`：解析层，把字符串或文件解析成 `CsvDocument`（字段经 `intern_raw` 入池）。
- `CsvWriter`：序列化层，把 `CsvDocument` 写成字符串或文件。

数据模型不保存原始逗号、引号、换行风格，也不保存未加引号字段两侧的格式空白策略。
这些属于读写策略，由 `CsvReaderOptions` 和 `CsvWriterOptions` 控制。这样模型保持稳定，
Reader/Writer 可以分别演进。

## Arena 架构与 ca::str 的集成（v1.3）

**v1.3 起，数据模型采用 Arena 架构**（与 `libca/toml`、`libca/json`、`libca/ini` 统一）：

- `CsvDocument` 内嵌 `Utf8StringArena`，所有字段经 `arena.intern_raw(...)` 入池。
- `CsvRow` / `CsvDocument` 的字段、标题行存 `Utf8StringRef`（16 字节、可拷贝、无独立堆分配）。
- 字段 ref 生命周期绑定 CsvDocument：document 析构/clear/move-assign 后所有 ref 失效。
- `CsvDocument` 禁拷贝（含 arena，不可共享），仅可移动。

IO 边界与 json/ini/toml 一致：

- `CsvReader::read` 接受 `Utf8StringRef`（零拷贝指向原文本）。
- `CsvWriter::write` 返回 `Utf8String`。
- 错误用 `ParseError{SourceLocation, Utf8String}`（位置 + 消息）。
- 文件路径参数用 `Utf8StringRef`。

### 为什么 csv 用 intern_raw（不校验 UTF-8）

CSV（RFC 4180）是字节级格式，不规定编码，字段可能含任意字节（含非 UTF-8）。
`Utf8StringArena::intern` 会校验 UTF-8，对非 UTF-8 字节会失败。本模块用
`Utf8StringArena::intern_raw`（不校验 UTF-8，按原始字节入池，码点数取保守值=字节长度）——
这正是 `libca/str`（PR #151）新增 `intern_raw` 为此类字节级格式预留的入口。

### 历史背景：为什么 v1.2 用 std::string，v1.3 改回 Utf8StringRef

v1.2 时数据模型用 `std::string`，理由记录在案：

- CSV 字段不保证 UTF-8，`Utf8String` 构造时校验 UTF-8 会在非 UTF-8 字段抛异常。
- `Utf8String` 不可拷贝，`CsvRow({...})` 初始化列表、`rows[i][j]` 返回值等易用 API 失效。

v1.3 这两点障碍已被解除：
- `intern_raw`（PR #151）解决了第一点——不校验 UTF-8 按原始字节入池。
- `Utf8StringRef`（toml 已验证）可拷贝，解除了第二点——`CsvRow({...})` 和 `rows[i][j]` 直接可用。

迁移收益：消灭大表每字段一次堆分配（换成 arena 64KB chunk），字段去重 bonus（CSV 字段
重复率高），析构一次性释放 arena chunk，与 json/ini/toml 的存储模型完全统一。

### CsvWriter 输出非 UTF-8 字段

`CsvWriter::write` 返回 `Utf8String`。`Utf8String` 的标准构造函数校验 UTF-8，因此
writer 输出非 UTF-8 字段需要绕过校验。本模块提供 `CsvWriterOptions::validate_utf8`
开关（默认 `true`，保持旧行为）：

- `validate_utf8 = true`（默认）：输出经标准 `Utf8String(const u8*, usize)` 构造，校验
  UTF-8；字段含非法字节时抛 `std::runtime_error`。
- `validate_utf8 = false`：输出经 `Utf8String::from_data_unchecked` 构造，**不校验 UTF-8**，
  按原始字节保留，与 `CsvDocument::intern_raw` 入池语义对齐。代价：Utf8String 的码点数
  length 取保守值（= 字节长度），按码点迭代行为不准——但 CSV 字段就是字节序列，按码点
  迭代本来也不是 CSV 的典型用法。

`Utf8String::from_data_unchecked` 是 `libca/str` 配套新增的工厂方法（与
`Utf8StringArena::intern_raw` 对称），供任何需要按原始字节保留的场景使用。

## 解析策略

Reader 支持常见 RFC 4180 行为：

- 逗号分隔字段。
- 双引号字段。
- 字段内双引号用两个连续双引号表示。
- 支持 CRLF、LF，以及 quoted field 内部换行。
- 可选择第一行作为标题行。

默认情况下未加引号字段原样保留；如果调用方需要兼容旧 `CsvFile` 的 trim 行为，可以打开
`trim_unquoted_space`。

## 写出策略

Writer 默认只在必要时加引号。字段中包含分隔符、引号、换行，或者首尾有空格/制表符时，
会自动加引号并转义字段内引号。行结束符默认是 `\n`，可以改为 `\r\n`。

## 错误模型（v1.2）

读文件和解析使用 `Result<CsvDocument, ParseError>` 返回错误。`ParseError` 含
`SourceLocation`（1-based 行 + 列）和 `Utf8String` 消息，便于调用方定位输入问题
（如 quoted field 内换行导致的未闭合，行号会反映跨行位置）。Writer 的字符串序列化不失败，
文件写出用 `Result<void, Utf8String>` 表达打开或写入失败。

## 与旧 utility 的关系

旧 `utility/CsvFile` 只支持简单按逗号拆行，无法正确处理 quoted comma、字段内引号、
多行字段。新模块迁移为独立库后，CSV 能力集中在 `libca/csv`，后续不再在 utility 下扩展。
