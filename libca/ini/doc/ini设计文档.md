---
version: 2.1
update:
2026-07-19 - v2.1 迁移到 Arena 架构：IniDocument 内嵌 Utf8StringArena，字段与索引统一 Utf8StringRef
2026-07-19 - v2.0 重写：深度集成 ca::str、新增类型化访问、修复带引号 value 改写 bug、
             错误带行列、重复检测、LineRecord 移入 detail 命名空间
2026-07-14 - 删除冗余英文摘要 design.md 与使用文档（并入 README），本文成为 ini 唯一设计文档
2026-07-06 - 首版，说明 INI 独立模块的保格式模型、Reader/Writer 分层与注释保留策略
---

# libca/ini 设计文档

## 定位

`libca_ini` 是独立的 INI 配置读写模块，替代旧 `utility` 中的 `IniFile`。核心目标不是只把
INI 读成 map，而是支持"读入、修改少量 key、写回"时保留人工维护的注释、空行和顺序。

模块命名空间是 `ca::ini`，构建目标是 `libca_ini`，单元测试目标是 `libca_ini_unittest`。
依赖 `libca_core` 和 `libca_str`。

## v2.0 / v2.1 重写动机

v1 能正确保格式 round-trip，但作为通用 INI 库有几个明显缺口。v2.0 针对性地重写：

| 痛点 | v2.0 应对 |
|------|-----------|
| 类型化访问缺失（`get` 只返字符串） | 新增 `get_int/get_double/get_bool/get_or`，自动剥引号后转换 |
| 错误信息薄、无列号 | 改用 `ParseError{SourceLocation, Utf8String}`，带 1-based 行（列） |
| 带引号 value 改写丢引号（潜在 bug） | `LineRecord` 记录 `value_quoted`/`value_quote_char`，set 重建时补回 |
| 与 ca::str 零集成、重复造轮子 | 全模块 `std::string` → `Utf8String`/`Utf8StringRef` |
| 同名 section/key 语义含糊 | `IniReaderOptions` 提供 `DuplicatePolicy::KeepLast`(默认)/`Error` |
| `LineRecord` 公开泄露 ABI | 移入 `ca::ini::detail` 命名空间并标注内部 |
| 从零生成格式不可控 | `IniWriterOptions::line_ending` 统一换行；新行格式仍固定为 `key = value` |

向后兼容：保格式 round-trip、`(section, key)` 寻址、全局 key 用空串 section、同名取最后一个
等 v1 行为全部保留。

**v2.1 迁移到 Arena 架构**（与 `libca/toml`、`libca/json` 统一）：

- `IniDocument` 内嵌 `Utf8StringArena`，所有字符串字段（`IniLine` 的 section/key/value、
  `LineRecord` 的 raw/line_ending/key_prefix 等）从 `Utf8String` 改为 `Utf8StringRef`，
  指向内部 arena。
- 内部索引从 `std::map<std::string, ...>` 改为 `std::map<Utf8StringRef, ...>`（Utf8StringRef
  可拷贝可比较，直接做 key），**消除所有 `to_std()` 转换**。
- 类型化访问（`get`/`get_or`/`sections`/`keys`）返回 `Utf8StringRef`（生命周期绑定 document）。
- 错误通道（`get_int`/`get_double`/`get_bool` 的错误）仍用 owning `Utf8String`——错误要走出
  document 生命期，必须是拥有式。

核心动机：消灭零散堆分配。v2.0 每行 11 个 Utf8String 字段各一次堆分配，改为 Arena 后所有字符串
进 arena 的 64KB chunk，析构 = arena 释放几个 chunk。附带 bonus：`intern` 自动去重，
section/key 天然去重省内存。

## 与 ca::str 的集成（Arena 架构）

本模块深度集成 `ca::str`，**v2.1 起采用 Arena 架构**：

- 输入文本用 `Utf8StringRef`（非拥有视图，零拷贝指向调用方持有的原文本）。
- 行记录、key、value 等字符串字段用 `Utf8StringRef`（指向所属 `IniDocument` 内的 arena）。
- 错误消息用 `Utf8String`（拥有所有权——错误要走出 document 生命期）。

代价是 `IniDocument`（含 arena，不可共享）与 `ParseError`（含 owning `Utf8String`）都是
move-only。`IniReader::read` 返回 `Result<IniDocument, ParseError>`，调用方用
`std::move(result).unwrap()` 取值。配套地，`libca/core/result.hpp` 补了 `unwrap_err() &&`
move 重载（与 `unwrap() &&` 对称），以支持 move-only 错误类型。

内部索引用 `std::map<Utf8StringRef, ...>`（键是 Utf8StringRef，指向 arena 内副本）——
Utf8StringRef 可拷贝可比较，直接做 map key，无需 `to_std()` 转换。

## 架构

模块分为三层：

- `IniDocument`：保格式数据模型，内部保存按文件顺序排列的行节点（`detail::LineRecord`）。
- `IniReader`：把字符串或文件解析成 `IniDocument`。
- `IniWriter`：按行节点顺序写回字符串或文件。

`IniDocument` 内部同时维护两个视角：一个是用于写回的原始行记录（`records_`），另一个是
用于查询的 section/key 索引（`section_index_` / `key_index_`）。读取后不修改的行原样写回；
调用 `set/remove` 后，只重建受影响的 key/value 行或 section 范围。

## 行节点模型

每一行被分类为：空行、注释行、section 行、key/value 行（`IniLineKind`）。

公开的 `IniLine`（kind/section/key/value 只读视图）用于观察文档结构。内部的
`detail::LineRecord` 额外保存重建该行所需的所有格式片段：key 前缩进、key 与分隔符间空白、
分隔符（`=` 或 `:`）、分隔符后空白、行内注释后缀、行原始文本、行结束符。**新增
`value_quoted` / `value_quote_char`**：解析时记录 value 是否被首尾配对的引号包裹，set 重建
该行时据此补回引号——这是修复 v1 带引号 value 改写 bug 的关键。

`detail::LineRecord` 在 `ca::ini::detail` 命名空间，标注为内部实现细节、不保证稳定，避免
ABI 泄露到公开接口。

## 注释保留策略

INI 的注释保留是本模块的核心约束：

- 独立注释行原样保留。
- 空行原样保留。
- 未修改 key/value 行原样保留。
- 修改 value 时保留 key 左侧缩进、分隔符、分隔符后空白、行内注释，并按 `value_quoted`
  补回引号。
- 新增 key 插入到对应 section 末尾、下一个 section 前面，用统一的 `key = value` 格式。

删除 section 时会删除 section 头以及直到下一个 section 前的所有行，包括属于该 section 的
注释和空行。

## 类型化访问

`get` 返回原始字符串值（含可能的首尾引号，向后兼容 v1）。`get_int/get_double/get_bool`
内部先剥首尾配对的单/双引号再转换：

- `get_int`：用 `std::stoll`，要求整串都是合法整数。
- `get_double`：用 `std::stod`。
- `get_bool`：接受 `true/false/yes/no/on/off/1/0`（大小写不敏感）。
- `get_or`：找不到时返回默认值。

转换失败或 key 不存在返回 `Err`（消息为 `Utf8String`）。

## 错误模型

- 解析错误用 `ParseError{SourceLocation, Utf8String}`，`SourceLocation` 含 1-based 行（+列，
  当前列号信息在行级，精确到行的错误已够定位配置问题）。
- 不用异常，全程经 `Result<T, ParseError>` 传播。
- `IniReader::read_file` 打开失败也返回 `ParseError`。
- `IniWriter::write` 字符串序列化不失败；`write_file` 用 `Result<void, Utf8String>` 表达
  打开/写入失败。

## 重复 section / key 处理

`IniReaderOptions`：

- `on_duplicate_section`：`KeepLast`（默认，v1 行为）/ `Error`。
- `on_duplicate_key`：同上。

`KeepLast` 时，索引指向最后出现的行（查询返回最后一个值，与 v1 一致）；`Error` 时首个重复
即报错带行号。

## 行内注释识别

`find_inline_comment` 在 value 部分扫描注释起始符（`#` 或 `;`，受 `hash_comment`/
`semicolon_comment` 控制），考虑单/双引号和反斜杠转义。`inline_comment_strict_whitespace`
（默认 true）要求注释符前是空白或位于 value 起始；设为 false 时任何位置的注释符都算注释
（更宽松，贴近部分 INI 实现）。

## 与旧 utility 的关系

旧 `utility/IniFile` 读入后只保留 section/key/value，写回会丢失注释、空行和原始顺序。
v1 迁移为独立库后修复了保格式问题；v2.0 进一步补齐类型化访问、ca::str 集成和错误模型，
INI 能力集中在 `libca/ini`。

## 已知限制

- 全量解析（无流式）：INI 配置文件都很小，不投入流式能力。
- 新增 key 格式固定为 `key = value`：无法配置 `:` 分隔或自定义缩进；如需可后续给 `set`
  加格式重载。
- 全局区用空字符串 section 的约定保留（向后兼容）；内部已用 std::string 索引键，魔法值
  撞车风险局限于"用户真有一个名字为空的 section"这种异常输入。
