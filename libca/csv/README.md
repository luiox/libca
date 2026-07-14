# libca_csv

独立 CSV 文本读写模块。命名空间 `ca::csv`，构建目标 `libca_csv`。

> 设计与格式策略见 `doc/csv设计文档.md`；以下为快速示例。接口签名见头文件 Doxygen 注释。

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

Writer 默认只在必要时加引号：字段含分隔符、引号、换行，或首尾有空格/制表符时自动加引号，
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

新模块默认保留未加引号字段原文（旧 `utility/CsvFile` 会修剪两侧空白）。需要兼容旧行为时：

```cpp
CsvReaderOptions options;
options.trim_unquoted_space = true;
```
