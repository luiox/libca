---
version: 1.2
update:
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

- `CsvDocument` / `CsvRow`：数据模型层，保存可选标题行和记录行。
- `CsvReader`：解析层，把字符串或文件解析成 `CsvDocument`。
- `CsvWriter`：序列化层，把 `CsvDocument` 写成字符串或文件。

数据模型不保存原始逗号、引号、换行风格，也不保存未加引号字段两侧的格式空白策略。
这些属于读写策略，由 `CsvReaderOptions` 和 `CsvWriterOptions` 控制。这样模型保持稳定，
Reader/Writer 可以分别演进。

## 与 ca::str 的集成（v1.2）

本模块在 IO 边界接入 `ca::str`，与 `libca/json`、`libca/ini` 风格一致：

- `CsvReader::read` 接受 `Utf8StringRef`（零拷贝指向原文本）。
- `CsvWriter::write` 返回 `Utf8String`。
- 错误用 `ParseError{SourceLocation, Utf8String}`（位置 + 消息）。
- 文件路径参数用 `Utf8StringRef`。

**数据模型内部仍用 `std::string`**（`CsvRow` / `CsvDocument` 的字段、标题行）。这是与
json/ini 的有意差异：

- CSV（RFC 4180）是字节级格式，不规定编码，字段可能含任意字节（含非 UTF-8）。
  强行用 `Utf8String`（构造时校验 UTF-8）会在遇到非 UTF-8 字段时抛异常，反而限制能力。
- `Utf8String` 不可拷贝，会让 `CsvRow({...})` 初始化列表、`rows[i][j]` 返回值等易用 API
  失效。CSV 作为表格数据，字段值的拷贝/比较是高频操作，保留 `std::string` 更合适。

因此 ca::str 的集成体现在"IO 边界统一"（输入输出与其它格式库一致），而非"内部类型统一"。
`SourceLocation` / `ParseError` 的定义与 json/ini 同形态，当前各自独立，等三个格式库都
稳定后再考虑回抽公共层。

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
