---
version: 1.0
update:
2026-07-26 - 首版，说明 yaml 配置子集的支持范围、解析器 (行,列) 游标架构、writer 保真规则与已知坑
---

# libca/yaml 设计文档

## 定位

`libca_yaml` 是 YAML **配置子集**文本读写模块（DOM 形态），是 libca 多格式读写库系列的第五份
（前四份为 json / ini / csv / toml）。命名空间 `ca::yaml`，构建目标 `libca_yaml`，单元测试
目标 `libca_yaml_unittest`。依赖 `libca_core` 和 `libca_str`，不依赖其它 libca 模块。

主要使用场景是**配置文件**：很多项目的配置就是 YAML（CI、服务、应用配置）。目标是能方便地读
一份手写的配置、改几个字段再写回，而不是实现完整 YAML 1.2。

## 为什么是"配置子集"而非完整 YAML

完整 YAML 1.2 规范极大（锚点/别名/标签/多文档/复杂键/多种 schema），实现和维护成本远超
配置读写的实际需要，且部分特性（别名展开、自定义标签）在配置场景是安全隐患（YAML 炸弹）。
经与需求方确认，采用**配置子集**：

- 手写解析器，零第三方依赖（与 toml/json 同构，不引入 libyaml/yaml-cpp）。
- 覆盖配置里真正会写的形态：块式 mapping/sequence、标量、单行 flow、块标量、注释。
- 用不到的高级特性**明确报错拒绝**（给清晰错误消息），而不是静默忽略——静默忽略会让用户
  以为生效了，配置错误无声化最危险。

支持范围表与拒绝清单见 README。

## 架构：与 toml 同构

复用 toml 本轮验证过的 Arena 架构，几乎 1:1 镜像：

- `YamlDocument` 根容器持有 `Utf8StringArena` + root，是整个 DOM 的所有权根，move-only。
- `YamlValue` DOM 节点存 `Utf8StringRef`（16 字节、可拷贝、无独立堆分配），字符串字节进
  arena，intern 去重。节点因此可拷贝（浅拷贝引用）。
- 错误 `Result<YamlDocument, ParseError>`，首错即停，带行/列。
- `source_location.hpp` / `parse_error.hpp` 各模块独立一份（不跨模块共享），照抄 toml。

**与 toml 的关键差异**：YAML 根可为任意节点（标量/序列/映射），所以 `YamlValue` 默认构造是
**Null**，`YamlDocument::root()` 默认 Null；toml 根固定是 Table。

### Mapping 存储（照抄 TomlValue::TableData）

`MappingData { vector<pair<Utf8StringRef,YamlValue>> members; unordered_map<string_view,usize> index; }`。
members 保插入序供遍历与写回，index 提供 key→下标 O(1) 查找。二者只经 `set/find/remove`
同步修改，`as_mapping()` **仅 const**（不暴露可变引用，防绕过索引改成员破坏一致性）。
index 的 `string_view` 指向 key 的 arena 字节（arena 不搬移，members 扩容不影响）。

## 解析器架构（关键设计）：行预扫 + (行, 列) 游标

YAML 是缩进敏感的面向行格式。解析器分两层：

### 1. `split_lines()` 行预扫

一次性把输入切成 `Line { offset, line_no, indent, text, length, blank, comment_only }`。
职责：跳 BOM；剥 `\r\n`（孤立 `\r` 报错）；算前导空格 indent（**缩进中出现 TAB 报错**——
YAML 禁止 TAB 缩进，且这是最常见的手写错误来源）；标记空行与纯注释行。

### 2. (行号, 行内字节列) 游标

游标 = 行号 `li_` + 行内字节列 `col_`。**核心技巧：消费 `- ` 前缀后 `col_` 前移，行剩余
部分作为一条"虚拟行"，其缩进即 `col_`。** 这样 `- scalar` / `- key: v` / `- - a` 三种紧凑
形式统一走同一套块解析逻辑，零特例：

```
- key: v
  ^col_=2 起，"key: v" 是缩进 2 的虚拟行 → 正常 parse_block_mapping
```

### 函数分解

- `parse_block_node(min_indent)`：分派器。dash 开头 → sequence；含 `key:`（首个后跟
  空白/行尾的 `:`）→ mapping；否则 scalar / flow / 块标量。
- `parse_block_sequence(n)`：同缩进 `-` 循环。`-` 后有内容 → 在行中递归
  `parse_block_node(n+1)`；`-` 独占一行 → 下一行更深缩进解析（无内容为 Null 项）。
- `parse_block_mapping(n)`：**key 检测 = 首个后跟空白或行尾的 `:`**——所以
  `url: http://x` 的 `://` 不断开、`a:b` 整体是标量。值同行给标量/flow；值空则看下一行：
  更深缩进 → 子块；同缩进 dash → **零缩进 sequence**（配置常见写法，见下）；否则 Null。
  重复 key 经 index O(1) 判定，报错。
- `parse_block_scalar(n)`：`|`（字面）/ `>`（折叠），带 `-`/`+` chomping。**直接操作原始行**
  （块标量内缩进里的 `#` 是内容不是注释）；首个非空行定基准缩进；`>` 折叠规则（单换行→
  空格、空行→换行、更深缩进行保留字面换行）。**显式缩进指示符 `|2` 报错**。
- `parse_single_quoted` / `parse_double_quoted`：仅单行。双引号支持
  `\0 \a \b \t \n \v \f \r \" \/ \\ \xXX \uXXXX \UXXXXXXXX`，`\uXXXX` 代理对合并成一个
  码点再 UTF-8 编码。
- `parse_flow_value/sequence/mapping`：行内递归下降，`[`/`{`/引号/plain；**禁尾逗号、禁跨行**。
- `reject_unsupported_indicator`：`&`/`*`/`!`/`%`/`? `/第二个 `---`/`...`/`<<` 各带明确
  错误消息拒绝。
- `DepthGuard` RAII + `max_depth=1000` 防深嵌套栈溢出。

### 类型解析 `resolve_plain_scalar`（parser 与 writer 共用）

**先形状检查、后 strtoll/strtod**（照抄 toml_parser.cpp 的 errno/endptr 纪律，防 strtod
吞 `inf`/前导空白）。返回 `ResolvedScalar { kind, boolean, integer, floating }`。
kind：Null / Boolean / Integer / Float / String / IntOverflow / FloatOverflow。

溢出时（IntOverflow/FloatOverflow）该标量**退化为 String**——YAML core schema 语义是
"无法解析为已知标量类型的即普通字符串"，与 toml 的"整数溢出报错"不同。

## Writer 实现要点

照抄 toml_writer 骨架（`Utf8StringBuilder` + 递归），缩进默认 2 可配。

- 非空集合一律块式；空集合 `{}`/`[]`；sequence 内 mapping 用 `- key:` 紧凑式（首个 key
  承接 `- ` 前缀、抑制自缩进）。
- **字符串加引号判定的承重规则：`resolve_plain_scalar(s).kind != String` 就必须加引号。**
  这一条保证 `"true"`/`"3.14"`/`"~"` 写出再读回仍是字符串——类型保真且与 parser 零重复
  逻辑（共用同一判定函数）。另：空串、首尾空白、指示符打头、含 `: `/` #`/控制字符也加引号
  （优先单引号，含控制字符用双引号转义）。
- 含 `\n` 的字符串 → `|`/`|-`/`|+`（按尾换行数选 clip/strip/keep）。当块式**不可保真**
  （含 `\r`/其它控制字符，或首个非空行以空白开头会让读回缩进检测误判）时**回退双引号转义**。
- 浮点整数形态（如 `1.0`）输出补 `.0`，避免 reparse 变整数；`.inf`/`-.inf`/`.nan`
  按 YAML 字面量输出（非 C 的 `inf`/`nan`）。

**保真目标**：write→read→DOM 结构相等。测试用 `dom_equal` 递归比较（NaN 视为相等）+
`expect_roundtrip` 助手覆盖。

## 测试

- `yaml_value_test.cpp`（10）：DOM 工厂/谓词、sequence、mapping 保序 + 索引一致性、
  remove 后索引一致、clone 深拷贝、document root-Null / clear / move。
- `yaml_reader_test.cpp`（63）：标量类型（含 Norway、`url: http://x`、`12:30:45`、i64 溢出
  退化 String）、mapping、sequence/紧凑形式、零缩进 sequence、引号、unicode 转义 + 代理对、
  flow、块标量 + chomping、注释布局、全部拒绝特性（`read_fails_with` 断言错误消息子串）、
  BOM/CRLF、错误定位。
- `yaml_writer_test.cpp`（17）：标量格式、引号判定矩阵、控制字符双引号、多行块标量 +
  chomping 选择、首行前导空白回退、golden 嵌套布局、紧凑 sequence-of-mappings、空集合、
  write_file/read_file、4 组 write→read→dom_equal 往返。

## 已知坑（实现期间踩到的）

1. **`ParseError` 含 move-only 的 `Utf8String message`**——不能拷贝。测试助手取错误用
   `std::move(result).unwrap_err()`；`read_ok` 失败分支不能在 EXPECT 流里拷 message，需先
   `is_err()` 判定再 move 出来。
2. **`Utf8StringArena::intern` 是构建 DOM 的正道**——写测试/构建节点时字符串都要
   `doc.arena().intern(...)` 入池，`YamlValue::make_string` 收的是 `Utf8StringRef`。
3. **零缩进 sequence 例外**：`key:` 换行后 `- item` 与 key **同列**（indent 相等）在标准
   YAML 里合法且是最常见的配置写法，parser 在 mapping 值检测里显式允许"同缩进 dash → 该
   key 的 sequence 值"，不当作 mapping 结束。
4. **单行引号跨行是非法的**：单/双引号字符串仅限单行。测试里想要多行串必须用 `|` 块标量，
   不能写 `'line1\n\nline2'`（初版测试踩过，已改）。
5. **块标量保真回退**：首行以空白开头的多行串写成 `|` 块标量后读回缩进会误判，writer 检测到
   这种不可保真情形回退双引号——写测试断言时要照顾这条分支。
6. **MSVC 工具链**：`xmake f -p windows -a x64 -y --with_tests=y --with_em=n`，不指定平台
   会回落 mingw、gtest 编译失败。单目标构建用 `xmake build libca_yaml_unittest`（xmake
   一次只接受一个目标），全量测试用 `xmake test -g libs/test`。
