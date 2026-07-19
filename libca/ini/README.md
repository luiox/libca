# libca_ini

独立 INI 配置读写模块。命名空间 `ca::ini`，构建目标 `libca_ini`。

核心目标是支持「读入、修改少量 key、写回」时保留人工维护的注释、空行和顺序，而不是只把
INI 读成 map。采用 **Arena 架构**：`IniDocument` 内嵌 `Utf8StringArena`，所有字符串字段
（`IniLine` / `LineRecord`）存 `Utf8StringRef`，析构 document 时 arena 一次性释放，无零散堆
分配。输入 `Utf8StringRef`，值 `Utf8StringRef`（生命周期绑定 document）。

> 设计与保格式策略见 `doc/ini设计文档.md`；以下为快速示例。接口签名见头文件 Doxygen 注释。

## 引入

```cpp
#include "libca/ini/ini.hpp"

using namespace ca::ini;
```

构建时依赖 `libca_ini`、`libca_str`、`libca_core`。

> 下文示例中 `R("...")` 是 `ca::str::Utf8StringRef::from_cstr("...")` 的简写，仅为排版紧凑。

## 读取配置

```cpp
auto result = IniReader::read_file(Utf8StringRef::from_cstr("app.ini"));
if (result.is_err()) {
    auto err = std::move(result).unwrap_err();
    // err.location.line / err.message
}

IniDocument document = std::move(result).unwrap();
Utf8StringRef host = document.get(
    Utf8StringRef::from_cstr("server"),
    Utf8StringRef::from_cstr("host")).unwrap();
// host 是 Utf8StringRef，指向 document 内部 arena。document 析构后失效。
```

全局 key 使用空字符串作为 section 名：

```cpp
Utf8StringRef value = document.get(
    Utf8StringRef::from_cstr(""),
    Utf8StringRef::from_cstr("root_key")).unwrap();
```

`IniDocument` 禁拷贝、仅移动（含 arena，所有权清晰）。

## 类型化访问

`get` 返回原始字符串引用（含可能的首尾引号）。需要 int/double/bool 时用类型化访问，
会自动剥首尾配对引号后转换：

```cpp
auto port = document.get_int(R("server"), R("port")).unwrap_or(80);
auto debug = document.get_bool(R("server"), R("debug")).unwrap_or(false);
auto ratio = document.get_double(R("server"), R("ratio")).unwrap_or(0.0);
Utf8StringRef name = document.get_or(R("server"), R("name"), R("default"));
```

`get_bool` 接受 `true/false/yes/no/on/off/1/0`（大小写不敏感）。

## 修改并写回

```cpp
auto result = IniReader::read(Utf8StringRef::from_cstr(
    "# comment\n"
    "[server]\n"
    "host = 127.0.0.1 ; keep inline\n"));
IniDocument document = std::move(result).unwrap();

document.set(Utf8StringRef::from_cstr("server"),
             Utf8StringRef::from_cstr("host"),
             Utf8StringRef::from_cstr("0.0.0.0"));
document.set(Utf8StringRef::from_cstr("server"),
             Utf8StringRef::from_cstr("port"),
             Utf8StringRef::from_cstr("8080"));

Utf8String text = IniWriter::write(document);
```

写回时，原有注释、空行和未修改行会保持原样。上例中 `; keep inline` 会保留在 `host`
这一行后面。**带引号的 value 修改后保留原引号风格**：`host = "127.0.0.1"` 改成
`host = "0.0.0.0"`（引号不丢）。

## 删除配置

```cpp
document.remove(R("server"), R("port"));
document.remove_section(R("debug"));
```

删除 section 会删除 section 头以及该 section 下直到下一个 section 前的所有行。

## 选项

```cpp
IniReaderOptions opts;
opts.allow_global_keys = false;        // 禁止 section 前的全局 key
opts.on_duplicate_section = DuplicatePolicy::Error;  // 重复 section 报错（默认保留最后一个）
opts.on_duplicate_key = DuplicatePolicy::Error;      // 重复 key 报错（默认保留最后一个）
opts.inline_comment_strict_whitespace = true;  // 紧贴 value 的 #/; 不算注释（默认）

IniWriterOptions wopts;
wopts.line_ending = "\n";  // 统一输出 LF；默认空表示保留各行的原换行符
```
