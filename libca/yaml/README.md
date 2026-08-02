# libca_yaml

YAML **配置子集**读写模块（DOM 形态）。命名空间 `ca::yaml`，构建目标 `libca_yaml`。

手写解析器，零第三方依赖，方便替换手写配置文件。深度集成 `ca::str`：输入接受
`Utf8StringRef`（零拷贝指向原文本），DOM 字符串值用 `Utf8StringRef` 指向所属
`YamlDocument` 内部的 `Utf8StringArena`。错误用 `Result<YamlDocument, ParseError>`，
不用异常。

> 这是**配置子集**，不是完整 YAML 1.2 实现。锚点/别名/标签/多文档/复杂键**明确报错拒绝**
> （不静默忽略）。支持范围与拒绝清单见下,设计与取舍见 `doc/yaml设计文档.md`;
> 接口签名见头文件 Doxygen 注释。

## 引入

```cpp
#include "libca/yaml/yaml.hpp"

using namespace ca::yaml;
```

构建时依赖 `libca_yaml`、`libca_str`、`libca_core`。

## 读取

```cpp
auto result = YamlReader::read(ca::str::Utf8StringRef::from_cstr(
    "title: YAML Example\n"
    "server:\n"
    "  host: localhost\n"
    "  port: 8080\n"));
if (result.is_err()) {
    auto err = std::move(result).unwrap_err();
    // err.location.line / .column / err.message
    return;
}

YamlDocument doc = std::move(result).unwrap();
const YamlValue* host = doc.root()
    .find(ca::str::Utf8StringRef::from_cstr("server"))
    ->find(ca::str::Utf8StringRef::from_cstr("host"));
if (host != nullptr) {
    // host->as_string() == "localhost"
}
```

## Arena 架构（与 json/ini 的差异，同 toml）

`YamlDocument` 持有 `ca::str::Utf8StringArena` + root `YamlValue`。DOM 节点内的字符串
（string 值、mapping key）都是 `Utf8StringRef`，指向 arena 内去重后的字节副本。

- 析构 = arena 释放几个 chunk，零散分配清零。
- 相同字符串只存一份（intern 去重）。
- `YamlValue` 因此**可拷贝**（浅拷贝引用，不像 json 的 `JsonValue` 是 move-only）。
- root 默认是 **Null**（YAML 根可为任意节点：标量/序列/映射），不像 toml 根固定是 Table。

**生命周期约束**：节点内 `Utf8StringRef` 绑定到所属 `YamlDocument`。document 析构后引用
失效。需要长期持有请拷出 `Utf8String`：

```cpp
const auto& view = doc.root().find(...)->as_string();
ca::str::Utf8String owned(view.data(), view.byte_length());  // 独立副本
```

## 构建与写回

```cpp
YamlDocument doc;
doc.root() = YamlValue::make_mapping();
doc.root().set(doc.arena().intern("name"),
               YamlValue::make_string(doc.arena().intern("libca")));

YamlValue server = YamlValue::make_mapping();
server.set(doc.arena().intern("host"),
           YamlValue::make_string(doc.arena().intern("localhost")));
server.set(doc.arena().intern("port"), YamlValue::make_integer(8080));
doc.root().set(doc.arena().intern("server"), std::move(server));

ca::str::Utf8String text = YamlWriter::write(doc);
// name: libca
// server:
//   host: localhost
//   port: 8080
```

`YamlWriter::write` 输出块式 YAML：非空 mapping/sequence 一律块式换行缩进，空集合用
flow `[]`/`{}`，sequence 内的 mapping 用 `- key:` 紧凑式。字符串**按需加引号**——若不加
引号会被读成非字符串类型（如 `true`/`3.14`/`~`）或破坏结构则强制加引号，保证
write→read 类型保真。含换行的字符串输出为 `|` 字面块标量。

## 读写文件

```cpp
auto read_result  = YamlReader::read_file(ca::str::Utf8StringRef::from_cstr("config.yaml"));
auto write_result = YamlWriter::write_file(ca::str::Utf8StringRef::from_cstr("out.yaml"), doc);
```

## 支持的 YAML 子集

- **块式 mapping/sequence**：缩进嵌套（仅空格，缩进含 TAB 报错）；`- key: v` 紧凑形式；
  `key:` 下**零缩进 sequence**（`- item` 与 key 同列，常见配置写法）。
- **标量类型解析按 YAML 1.2 core schema**：
  - `null` / `~` / 空值 → Null
  - `true` / `false`（含 `True`/`TRUE` 等大小写变体）→ Boolean
  - 十进制 / `0x` / `0o` 整数 → Integer（i64）
  - 浮点、科学计数、`.inf` / `-.inf` / `.nan` → Float（f64）
  - 其余 → String
- **单/双引号字符串**（仅单行）：单引号 `''` 转义；双引号 `\n \t \r \" \\ \/ \0 \xXX
  \uXXXX \UXXXXXXXX`（含代理对合并）。
- **行内 flow**：`[a, b, c]` / `{k: v}`，单行、可嵌套；**禁尾逗号**。
- **注释** `#`：整行与行尾（引号内、块标量内不算注释）。
- **块标量** `|`（字面）/ `>`（折叠），带 `-`（strip）/ `+`（keep）chomping 指示符。
- 开头单个 `---` 文档标记允许；BOM 跳过；CRLF 支持。
- mapping **重复 key 报错**（配置安全，不取后者覆盖前者）。

## 明确拒绝（报错，不静默忽略）

配置场景用不到、且容易误用或引入安全问题的特性一律报错并给出清晰消息：

| 特性 | 示例 | 原因 |
|------|------|------|
| 锚点 / 别名 | `&a` / `*a` | 配置不需要引用共享，易成 YAML 炸弹 |
| 标签 | `!!str`、`!Foo` | 无自定义类型系统 |
| 多文档 | 第二个 `---` / `...` | 配置是单文档 |
| 复杂键 | `? [a, b]:` | 配置 key 皆为标量 |
| 合并键 | `<<: *a` | 依赖别名 |
| 显式缩进指示符 | `\|2` | 缩进自动检测已足够 |
| `%` 指令 | `%YAML 1.2` | 不做版本协商 |

## number 的 int/float 判定

与 toml 一致，严格区分整数与浮点：字面量形状不含 `.`/`e`/`E`/`.inf`/`.nan` 且能整解析为
i64 → `Integer`；否则按浮点解析为 `Float`。整数超 i64 范围或浮点越界时该标量**退化为
String**（YAML core schema 语义：无法解析为标量类型的即普通字符串，不像 toml 那样报错）。

值得注意的“陷阱”（core schema 与旧 YAML 1.1 的关键差异）：

- **Norway problem**：`no` / `yes` / `on` / `off` **不是布尔**，一律是 String。只有
  `true`/`false` 及其大小写变体是 Boolean。
- `url: http://example.com` 中的 `://` 不断开 mapping（key 检测要求 `:` 后跟空白/行尾），
  值 `http://example.com` 是完整 String。
- `12:30:45`、`2026-07-26` 等**不做时间/日期特化**，是 String（配置里想要时间自己解析）。
