# libca_xml

XML **配置子集**读写模块（DOM 形态）。命名空间 `ca::xml`，构建目标 `libca_xml`。

手写解析器，零第三方依赖，方便读写 XML 配置文件。深度集成 `ca::str`：输入接受
`Utf8StringRef`（零拷贝指向原文本），DOM 字符串（元素名、属性名/值、文本内容）用
`Utf8StringRef` 指向所属 `XmlDocument` 内部的 `Utf8StringArena`。错误用
`Result<XmlDocument, ParseError>`，不用异常。

> 这是**配置子集**，不是完整 XML 1.0 处理器。命名空间不特殊处理（`prefix:local` 整体
> 作为名字）；DOCTYPE/DTD、自定义实体、非声明处理指令（PI）**明确报错拒绝**（不静默忽略）。
> 支持范围与拒绝清单见下，设计与取舍见 `doc/xml设计文档.md`；接口签名见头文件 Doxygen 注释。

## 引入

```cpp
#include "libca/xml/xml.hpp"

using namespace ca::xml;
```

构建时依赖 `libca_xml`、`libca_str`、`libca_core`。

## 读取

```cpp
auto result = XmlReader::read(ca::str::Utf8StringRef::from_cstr(
    "<config>\n"
    "  <server host=\"localhost\" port=\"8080\"/>\n"
    "</config>\n"));
if (result.is_err()) {
    auto err = std::move(result).unwrap_err();
    // err.location.line / .column / err.message
    return;
}

XmlDocument doc = std::move(result).unwrap();
const XmlNode* server = doc.root().first_element(ca::str::Utf8StringRef::from_cstr("server"));
if (server != nullptr) {
    const auto* port = server->attribute(ca::str::Utf8StringRef::from_cstr("port"));
    // *port == "8080"
}
```

## 节点模型

`XmlNode` 是统一的树节点，用 `XmlNodeType` 区分 4 种形态：

- **Element**：名字 + 属性（保序 + O(1) 索引）+ 子节点（可混合）。
- **Text**：文本内容（实体已解码）。
- **Comment**：注释 `<!-- ... -->`（保留为节点）。
- **Cdata**：CDATA 段 `<![CDATA[ ... ]]>`（内容原样，不解码实体）。

**混合内容**：一个元素的子节点可以是 Element/Text/Comment/Cdata 任意交错
（如 `<p>Hello <b>world</b>!</p>`）。常用导航：

```cpp
const XmlNode* child = el.first_element(R("host"));  // 首个同名元素子节点
ca::str::Utf8String v = el.text();                   // 拼接直接 Text/Cdata 子节点，返回独立副本
const auto* attr = el.attribute(R("port"));          // 属性值，未找到返回 nullptr
```

## Arena 架构（与 json/ini 的差异，同 toml/yaml）

`XmlDocument` 持有 `ca::str::Utf8StringArena` + 声明 + prolog/epilog + root 元素。DOM 节点
内的字符串都是 `Utf8StringRef`，指向 arena 内去重后的字节副本。

- 析构 = arena 释放几个 chunk，零散分配清零。
- `XmlNode` 因此**可拷贝**（浅拷贝引用）。
- root 默认是空 Text 节点（解析/构建前的中性态）；良构文档的 root 是唯一根元素。

**生命周期约束**：节点内 `Utf8StringRef` 绑定所属 `XmlDocument`。document 析构后引用失效。
需要长期持有请拷出 `Utf8String`（`text()` 返回值本身即独立副本）。

## 构建与写回

```cpp
XmlDocument doc;
auto& a = doc.arena();
doc.root() = XmlNode::make_element(a.intern("config"));

XmlNode server = XmlNode::make_element(a.intern("server"));
server.set_attribute(a.intern("host"), a.intern("localhost"));
server.set_attribute(a.intern("port"), a.intern("8080"));
doc.root().append_child(std::move(server));

ca::str::Utf8String text = XmlWriter::write(doc);
// <config>
//   <server host="localhost" port="8080"/>
// </config>
```

`XmlWriter::write` 缩进美化输出，但对**混合内容保真**：元素只要含 Text/Cdata 子节点就整体
行内输出（不加缩进/换行，避免破坏有意义的空白）；子节点全是元素/注释时才分行缩进。文本与
属性值按需转义，空元素输出自闭合 `<x/>`，声明按 `document.declaration()` 输出。

## 读写文件

```cpp
auto read_result  = XmlReader::read_file(ca::str::Utf8StringRef::from_cstr("config.xml"));
auto write_result = XmlWriter::write_file(ca::str::Utf8StringRef::from_cstr("out.xml"), doc);
```

## 支持的 XML 子集

- **元素**：开始/结束标签、自闭合 `<x/>`、任意嵌套；闭合标签必须与开始标签匹配。
- **属性**：单/双引号值、保序、同名重复报错。
- **文本 + 混合内容**：文本与子元素任意交错。
- **注释**：`<!-- ... -->`，保留为 Comment 节点（prolog/epilog 中的注释存到
  `document.prolog()`/`epilog()`）。
- **CDATA**：`<![CDATA[ ... ]]>`，内容原样保留、不解码实体。
- **实体**：命名 `&lt; &gt; &amp; &apos; &quot;` + 数字字符引用 `&#DD;` / `&#xHH;`
  （解码为 UTF-8）。
- **XML 声明**：`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>`（存到
  `document.declaration()`）。
- UTF-8 BOM 跳过；CRLF 支持；Unicode 元素名/内容。
- **trim_whitespace**（默认开）：丢弃元素之间的纯空白文本节点，得到干净的树；含非空白字符的
  文本节点永远完整保留（混合内容不受影响）。需逐字节保真时置 false。

## 明确拒绝（报错，不静默忽略）

配置场景用不到、且容易误用或引入安全问题的特性一律报错并给出清晰消息：

| 特性 | 示例 | 原因 |
|------|------|------|
| DOCTYPE / DTD | `<!DOCTYPE ...>` | 配置不需要文档类型定义，且是 XXE / 实体炸弹入口 |
| 自定义实体 | `&myEntity;` | 依赖 DTD，安全隐患 |
| 处理指令 | `<?php ... ?>` | 除 XML 声明外不支持 PI |
| 多根元素 | `<a/><b/>` | XML 良构要求单根 |
| 属性内 `<` | `<a v="x<y"/>` | XML 良构禁止 |

## 命名空间说明

命名空间**不特殊处理**：`<ns:item xmlns:ns="...">` 中的元素名就是完整的 `ns:item`，
`xmlns:ns` 就是一个普通属性。若上层需要按命名空间语义处理，自行解析前缀即可。这满足绝大多数
配置文件（前缀固定、无需 URI 解析）。
