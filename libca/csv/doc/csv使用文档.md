---
version: 1.0
update:
2026-07-06 - 首版，补充 CSV Reader/Writer 常用用法
---

# libca/csv 使用文档

## 引入

```cpp
#include "libca/csv/csv.hpp"

using namespace ca::csv;
```

构建时依赖 `libca_csv`。

## 读取字符串

```cpp
CsvReaderOptions options;
options.first_row_is_header = true;

auto result = CsvReader::read("id,name\n1,Alice\n2,Bob", options);
if (result.is_err()) {
    // 处理 result.unwrap_err()
}

auto document = result.unwrap();
auto name = document.rows()[0][1];  // Alice
```

## 写出字符串

```cpp
CsvDocument document;
document.set_header({"id", "note"});
document.add_row(CsvRow({"1", "hello, \"csv\""}));
document.add_row(CsvRow({"2", "line1\nline2"}));

CsvWriterOptions options;
options.line_ending = "\r\n";

auto text = CsvWriter::write(document, options);
```

Writer 会自动处理逗号、引号和字段内换行。上面的 `note` 字段会按 CSV 规则加引号，
字段里的 `"` 会写成 `""`。

## 读写文件

```cpp
auto write_result = CsvWriter::write_file("data.csv", document);
if (write_result.is_err()) {
    // 处理写入失败
}

auto read_result = CsvReader::read_file("data.csv", options);
if (read_result.is_err()) {
    // 处理打开失败或格式错误
}
```

## 兼容旧 trim 行为

旧 `utility/CsvFile` 会修剪逗号拆分后的字段空白。新模块默认保留未加引号字段原文；
如果要兼容旧行为，可以打开：

```cpp
CsvReaderOptions options;
options.trim_unquoted_space = true;
```
