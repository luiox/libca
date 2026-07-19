# libca_toml

TOML 1.0 读写模块（DOM 形态）。命名空间 `ca::toml`，构建目标 `libca_toml`。

深度集成 `ca::str`：输入接受 `Utf8StringRef`（零拷贝指向原文本），DOM 字符串值用
`Utf8StringRef` 指向所属 `TomlDocument` 内部的 `Utf8StringArena`。错误用
`Result<TomlDocument, ParseError>`，不用异常。

> 设计与取舍见 `doc/toml设计文档.md`，开发接力文档见 `doc/dev_plan.md`；
> 以下为快速示例。接口签名见头文件 Doxygen 注释。

## 引入

```cpp
#include "libca/toml/toml.hpp"

using namespace ca::toml;
```

构建时依赖 `libca_toml`、`libca_str`、`libca_core`。

## 读取

```cpp
auto result = TomlReader::read(Utf8StringRef::from_cstr(R"(
title = "TOML Example"

[server]
host = "localhost"
port = 8080
)"));
if (result.is_err()) {
    auto err = std::move(result).unwrap_err();
    // err.location.line / .column / err.message
    return;
}

TomlDocument doc = std::move(result).unwrap();
const TomlValue* host = doc.root()
    .find(Utf8StringRef::from_cstr("server"))
    ->find(Utf8StringRef::from_cstr("host"));
if (host != nullptr) {
    // host->as_string() == "localhost"
}
```

## Arena 架构（与 json/ini 的差异）

`TomlDocument` 持有 `ca::str::Utf8StringArena` + root `TomlValue`。DOM 节点内的字符串
（string 值、table key）都是 `Utf8StringRef`，指向 arena 内去重后的字节副本。

- 析构 = arena 释放几个 chunk，零散分配清零。
- 相同字符串只存一份（intern 去重）。
- `TomlValue` 因此**可拷贝**（不像 json 的 `JsonValue` 是 move-only）。

**生命周期约束**：节点内 `Utf8StringRef` 绑定到所属 `TomlDocument`。document 析构后引用
失效。需要长期持有请拷出 `Utf8String`：

```cpp
const auto& view = doc.root().find(...)->as_string();
ca::str::Utf8String owned(view.data(), view.byte_length());  // 独立副本
```

## 构建与写回

```cpp
TomlDocument doc;
doc.root().set(Utf8StringRef::from_cstr("name"),
               TomlValue::make_string(Utf8StringRef::from_cstr("libca")));

TomlValue server = TomlValue::make_table();
server.set(Utf8StringRef::from_cstr("host"),
           TomlValue::make_string(Utf8StringRef::from_cstr("localhost")));
server.set(Utf8StringRef::from_cstr("port"), TomlValue::make_integer(8080));
doc.root().set(Utf8StringRef::from_cstr("server"), std::move(server));

Utf8String text = TomlWriter::write(doc);
```

`TomlWriter::write` 输出符合 TOML 1.0 规范的文本：顶层 key=value 在前，子表以 `[a.b]`
表头分段在后，数组表用 `[[a.b]]`，每个元素一段。

## 读写文件

```cpp
auto read_result = TomlReader::read_file(Utf8StringRef::from_cstr("config.toml"));
auto write_result = TomlWriter::write_file(Utf8StringRef::from_cstr("out.toml"), doc);
```

## 支持的 TOML 1.0 子集

- 基本类型：String（4 种：basic / literal / multiline basic / multiline literal）、
  Integer（含 hex/oct/bin 三进制前缀 + 下划线分隔）、Float（含 inf/nan）、Boolean。
- datetime 4 种变体：offset date-time、local date-time、local date、local time
  （用 `TomlDatetime` 结构 + `Kind` 枚举区分，不降级为 string）。
- 复合类型：Array、Table（standard `[a.b.c]` / inline `{ x = 1 }` / array of tables `[[x]]`）。
- dotted keys、quoted keys。
- 整数溢出 i64 报错（与 TOML 严格语义一致，不降级 float）。
- 重复 key / 重复 table header / inline table 不可变约束违反 → 全部报错。
- 允许 UTF-8 BOM。

## number 的 int/float 判定

TOML 严格区分整数与浮点：字面量不含 `.`/`e`/`E` 解析为 `Integer`（i64），否则为 `Float`（f64）。
整数超 i64 范围**报错**（与 TOML 严格语义一致，不静默降级 float）。
