---
version: 1.1
update:
2026-07-14 - 删除冗余英文摘要 design.md 与使用文档（并入 README），本文成为 ini 唯一设计文档
2026-07-06 - 首版，说明 INI 独立模块的保格式模型、Reader/Writer 分层与注释保留策略
---

# libca/ini 设计文档

## 定位

`libca_ini` 是独立的 INI 配置读写模块，替代旧 `utility` 中的 `IniFile`。核心目标不是只把
INI 读成 map，而是支持“读入、修改少量 key、写回”时保留人工维护的注释、空行和顺序。

模块命名空间是 `ca::ini`，构建目标是 `libca_ini`，单元测试目标是 `libca_ini_unittest`。

## 架构

模块分为三层：

- `IniDocument`：保格式数据模型，内部保存按文件顺序排列的行节点。
- `IniReader`：把字符串或文件解析成 `IniDocument`。
- `IniWriter`：按行节点顺序写回字符串或文件。

`IniDocument` 内部同时维护两个视角：一个是用于写回的原始行记录，另一个是用于查询的
section/key 索引。读取后不修改的行会原样写回；调用 `set/remove` 后，只重建受影响的
key/value 行或 section 范围。

## 行节点模型

每一行会被分类为：

- 空行。
- 注释行。
- section 行。
- key/value 行。

注释行和空行保留原始文本与原始换行。key/value 行额外保存 key、value、分隔符、分隔符
周围空白、行内注释后缀。修改 value 时，Writer 会复用这些格式片段，保留行内注释。

## 注释保留策略

INI 的注释保留是本模块的核心约束：

- 独立注释行原样保留。
- 空行原样保留。
- 未修改 key/value 行原样保留。
- 修改 value 时保留 key 左侧缩进、分隔符、分隔符后空白和行内注释。
- 新增 key 插入到对应 section 末尾、下一个 section 前面。

删除 section 时会删除 section 头以及直到下一个 section 前的所有行，包括属于该 section
的注释和空行。这符合“删除整个配置块”的语义。

## 错误模型

Reader 使用 `Result<IniDocument, std::string>` 返回格式错误或文件打开错误。Writer 的字符串
序列化不失败，文件写出用 `Result<void, std::string>` 表达打开或写入失败。

## 与旧 utility 的关系

旧 `utility/IniFile` 读入后只保留 section/key/value，写回会丢失注释、空行和原始顺序。
新模块迁移为独立库后，INI 能力集中在 `libca/ini`，后续不再在 utility 下扩展。
