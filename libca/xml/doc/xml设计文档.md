---
version: 1.0
update:
2026-07-26 - 首版，说明 xml 配置子集的支持范围、统一节点模型、解析器字节游标架构、writer 混合内容保真规则与已知坑
---

# libca/xml 设计文档

## 定位

`libca_xml` 是 XML **配置子集**文本读写模块（DOM 形态），是 libca 多格式读写库系列的一员
（前有 json / ini / csv / toml / yaml）。命名空间 `ca::xml`，构建目标 `libca_xml`，单元测试
目标 `libca_xml_unittest`。依赖 `libca_core` 和 `libca_str`，不依赖其它 libca 模块。

主要使用场景是**配置文件**：不少项目（构建、服务、UI 布局）的配置是 XML。目标是能方便地读
一份手写配置、改几个字段/属性再写回，而不是实现完整 XML 1.0 + Namespaces + DTD 校验。

## 为什么是"配置子集"而非完整 XML

完整 XML 生态（DTD、实体声明、Namespaces、XSD/校验、XPath）体量巨大，且其中 DTD/外部实体
是 XXE、实体展开炸弹（billion laughs）等经典安全问题的入口，在配置场景不但用不到、还有害。
经与需求方确认，采用**配置子集**：

- 手写解析器，零第三方依赖（与 toml/yaml 同构，不引入 libxml2/pugixml）。
- 覆盖配置里真正会写的形态：元素/属性/文本、注释、CDATA、混合内容、XML 声明、标准实体。
- 用不到的高级特性**明确报错拒绝**（给清晰错误消息），而不是静默忽略。

三个关键取舍（与需求方确认）：

1. **命名空间不特殊处理**：`prefix:local` 整体作为名字，`xmlns:x` 是普通属性。配置里前缀
   固定、无需 URI 解析，拆分只会徒增 API 复杂度。
2. **注释保留为节点**：注释进 DOM（元素内为 Comment 子节点，root 之外进 prolog/epilog），
   write 时原样输出——方便"读改写"保留人工注释。
3. **支持混合内容**：元素的子节点可以是元素与文本交错（`<p>a <b>b</b> c</p>`），不强制
   "纯文本或纯子元素"。这带来 writer 的保真难题（见下）。

支持范围表与拒绝清单见 README。

## 节点模型：统一的 XmlNode

不像 toml/yaml 的"标量 + 容器"值模型，XML 是标签树，更自然的是**单一节点类型 + 形态枚举**：

`XmlNodeType { Element, Text, Comment, Cdata }`。`XmlNode` 内部用
`std::variant<ElementData, Utf8StringRef>`：Element 存 `ElementData`（名字 + 属性 + 子节点），
Text/Comment/Cdata 共用一个 `Utf8StringRef`（由 type_ 区分）。

- **属性**：`vector<pair<Utf8StringRef,Utf8StringRef>> + unordered_map<string_view,usize>`
  索引（照抄 toml TableData 模式），保插入序 + O(1) 查找/设置，同名覆盖不改顺序。
- **子节点**：`vector<XmlNode>`，支持混合内容；`first_element(name)` 跳过非元素找首个同名
  子元素，`text()` 拼接直接 Text/Cdata 子节点为独立 `Utf8String`。
- 字符串都是 `Utf8StringRef`（指向 arena），故 `XmlNode` 可拷贝；`clone()` 递归深拷贝结构。

**XmlDocument** 持有 arena + 声明 + prolog/epilog（root 之外的注释）+ root。root 默认空 Text
（中性态）；解析后是唯一根元素。move-only。

## 解析器架构：字节游标递归下降

XML 是定界符驱动（`<...>`）而非行/缩进驱动，所以用字节游标（照抄 TomlParser 的 pos_/loc_/
peek/advance/fail 模式），不需要 yaml 那套行预扫。

`run()` 顺序：跳 BOM → 可选 `<?xml ...?>` 声明 → prolog（空白 + 注释，DOCTYPE/PI 报错）→
唯一根元素 → epilog（空白 + 注释）→ 必须到 EOF（否则多根报错）。

核心函数：
- `parse_element`：`<` Name 属性* (`/>` 自闭合 | `>` 内容 `</Name>`)。结束标签名必须匹配
  开始标签，否则报 "mismatched closing tag: expected </x>, got </y>"。`DepthGuard` +
  `max_depth=1000` 防深嵌套栈溢出。
- `parse_attributes`：Name `=` 引号值；属性间要求空白；同名属性报错。
- `parse_content`：循环——`</` 结束；`<!--` 注释；`<![CDATA[` CDATA；`<!`/`<?` 报错；
  其它 `<` 递归子元素；否则累积文本。遇标签边界 flush 文本节点。
- `parse_reference`：`&#DD;`/`&#xHH;` 数字引用（累加时即时判 >0x10FFFF 越界）经 `encode_utf8`
  编码；命名实体仅 lt/gt/amp/apos/quot，其余报 "unknown entity reference"。
- 名字字符：ASCII 字母/数字/`_`/`-`/`.`/`:` + 所有 `>=0x80` 字节（宽松放行 Unicode 名字，
  不做完整 XML Name 产生式校验——配置够用）。

### trim_whitespace（默认开）

文本 flush 时，若该文本节点**全是** XML 空白（空格/\t/\r/\n）且 trim 开，则丢弃。这样
缩进美化过的元素树不会挂满纯空白文本节点，导航干净、writer 可重新缩进。**含任何非空白字符的
文本节点永远完整保留**（`Hello ` 的尾空格不丢），所以有意义的混合内容不受影响。需逐字节
保真时置 false。

代价：严格夹在两个兄弟元素之间的"有意义纯空白"（如 `<b>a</b> <b>c</b>` 的那个空格）在默认
模式下会丢。这在配置里极罕见；README 已注明，关 trim 即可保。

## Writer：混合内容保真的缩进

`XmlWriter::write` 缩进美化，但承重规则是**按"元素是否含文本"二分**：

- 元素**含任一 Text/Cdata 子节点** → 整体**行内**输出：`<p>...</p>` 内所有子节点无缩进/换行
  递归行内写。保护有意义的空白与文本不被重排破坏。
- 子节点**全是元素/注释** → **块**模式：`>` 换行，每个子节点缩进一层，最后 `</name>` 换行。
- **空元素** → 自闭合 `<x/>`。

与 reader 默认 trim 的配合是关键：解析得到的元素树没有纯空白文本节点，所以块模式加的缩进
空白在重新解析时被 trim 掉 → `write→read` 的 DOM 结构相等（测试用 `dom_equal` + 6 组
`expect_roundtrip` 覆盖）。混合内容元素走行内、零加空白，天然逐字节稳定。

转义：文本转 `& < >`；属性值转 `& < "` 及 `\t\n\r`（数字引用，双引号包裹）保真。CDATA/注释
内容原样输出（内含 `]]>`/`-->` 的极端情况不处理，配置不会出现）。

## 测试

- `xml_node_test.cpp`（8）：工厂/谓词、属性保序 + 索引 + 覆盖 + 删除一致性、混合内容 +
  导航 + text()、clone 深拷贝、document 默认态/构建/clear/move。
- `xml_reader_test.cpp`（28）：元素/属性/嵌套/重复子元素、命名实体 + 数字引用、注释（含
  prolog）、CDATA、混合内容、trim 开关、声明、BOM、Unicode 名字、全部拒绝路径
  （`read_fails_with` 断言消息子串）、错误定位。
- `xml_writer_test.cpp`（14）：golden 缩进布局、自闭合、属性、文本/属性转义、CDATA/注释、
  声明、write_file/read_file、6 组 write→read→dom_equal 往返（含混合内容、实体+CDATA、
  特殊属性、注释保留、带声明）。

## 已知坑（实现期间踩到的）

1. **`ParseError` 含 move-only 的 `Utf8String message`**——不能拷贝。reader 用 `clone_error`
   重建；测试助手取错误用 `std::move(result).unwrap_err()`。
2. **CDATA/注释内容用 `arena.intern_raw`** 而非 `intern`：它们可能含未校验字节（CDATA 原样），
   raw 版本不做 UTF-8 校验、按原始字节入池。
3. **混合内容 + trim + writer 三者耦合**：改任一处都要重跑 roundtrip。writer 的"含文本→行内"
   判定与 reader 的"块模式缩进会被 trim 掉"互为前提，破坏其一 roundtrip 就崩。
4. **数字字符引用越界要在累加中即时判**（`cp > 0x10FFFF`），不能等算完——溢出的十六进制串
   会绕过 u32。代理区 `0xD800..0xDFFF` 在 `encode_utf8` 里额外拒绝。
5. **名字放行 `>=0x80` 字节**支持 Unicode 元素名，但不做完整 Name 校验；这是配置子集的
   有意取舍，不是 bug。
6. **MSVC 工具链**：`xmake f -p windows -a x64 -y --with_tests=y --with_em=n`，不指定平台会
   回落 mingw、gtest 编译失败。单目标构建 `xmake build libca_xml_unittest`（xmake 一次只接受
   一个目标），全量测试 `xmake test -g libs/test`。
