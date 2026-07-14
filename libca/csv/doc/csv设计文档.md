---
version: 1.1
update:
2026-07-14 - 删除冗余英文摘要 design.md 与使用文档（并入 README），本文成为 csv 唯一设计文档
2026-07-06 - 首版，说明 CSV 独立模块的数据模型、Reader/Writer 分层与格式策略
---

# libca/csv 设计文档

## 定位

`libca_csv` 是独立的 CSV 文本读写模块，替代旧 `utility` 中的 `CsvFile`。它只负责
CSV 表格数据和文本格式之间的转换，不绑定业务 schema，也不依赖 `fs` 或 `str` 模块。

模块命名空间是 `ca::csv`，构建目标是 `libca_csv`，单元测试目标是
`libca_csv_unittest`。

## 架构

模块分为三层：

- `CsvDocument` / `CsvRow`：数据模型层，保存可选标题行和记录行。
- `CsvReader`：解析层，把字符串或文件解析成 `CsvDocument`。
- `CsvWriter`：序列化层，把 `CsvDocument` 写成字符串或文件。

数据模型不保存原始逗号、引号、换行风格，也不保存未加引号字段两侧的格式空白策略。
这些属于读写策略，由 `CsvReaderOptions` 和 `CsvWriterOptions` 控制。这样模型保持稳定，
Reader/Writer 可以分别演进。

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

## 错误模型

读文件和解析使用 `Result<T, std::string>` 返回错误。解析错误带有 line/column 信息，
便于调用方定位输入问题。Writer 的字符串序列化不失败，文件写出用
`Result<void, std::string>` 表达打开或写入失败。

## 与旧 utility 的关系

旧 `utility/CsvFile` 只支持简单按逗号拆行，无法正确处理 quoted comma、字段内引号、
多行字段。新模块迁移为独立库后，CSV 能力集中在 `libca/csv`，后续不再在 utility 下扩展。
