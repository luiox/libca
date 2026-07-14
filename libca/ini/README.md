# libca_ini

独立 INI 配置读写模块。命名空间 `ca::ini`，构建目标 `libca_ini`。

核心目标是支持「读入、修改少量 key、写回」时保留人工维护的注释、空行和顺序，而不是只把
INI 读成 map。

> 设计与保格式策略见 `doc/ini设计文档.md`；以下为快速示例。接口签名见头文件 Doxygen 注释。

## 引入

```cpp
#include "libca/ini/ini.hpp"

using namespace ca::ini;
```

构建时依赖 `libca_ini`。

## 读取配置

```cpp
auto result = IniReader::read_file("app.ini");
if (result.is_err()) {
    // 处理 result.unwrap_err()
}

auto document = result.unwrap();
auto host = document.get("server", "host").unwrap();
```

全局 key 使用空字符串作为 section 名：

```cpp
auto value = document.get("", "root_key");
```

## 修改并写回

```cpp
auto result = IniReader::read(
    "# comment\n"
    "[server]\n"
    "host = 127.0.0.1 ; keep inline\n");

auto document = result.unwrap();
document.set("server", "host", "0.0.0.0");
document.set("server", "port", "8080");

auto text = IniWriter::write(document);
```

写回时，原有注释、空行和未修改行会保持原样。上例中 `; keep inline` 会保留在 `host`
这一行后面。

## 删除配置

```cpp
document.remove("server", "port");
document.remove_section("debug");
```

删除 section 会删除 section 头以及该 section 下直到下一个 section 前的所有行。
