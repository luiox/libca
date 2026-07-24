# 提案：fs 模块 Unicode 路径封装

## 背景

`libca/fs` 的设计文档（`libca/fs/doc/fs设计文档.md` §1）已明确声明：字符串参数统一使用
`std::string`（UTF-8 语义）。但当前 `FileUtil` 的实现里，大多数方法用
`std::filesystem::path(std::string)` 直接从字节串构造路径。

这在 Windows 上是一个隐蔽的有损转换坑：

- `std::filesystem::path` 的 `std::string` 构造函数在 Windows 上按**本地代码页**
  （ACP，简体中文系统通常是 GBK/CP936）解析字节，而不是 UTF-8。
- 当路径包含中文文件名或其他非 ASCII 字符（例如 morpher 反混淆样本
  `bad-grunt的轻度混淆样本.jar`）时，字节会被按 GBK 误解码，导致路径不存在、
  打开失败或写入到错误的文件名。
- C++17 提供了 `std::filesystem::u8path(std::string)`，它在 Windows 上会把 UTF-8
  字节正确解码为 `wchar_t` 路径，是符合本模块"UTF-8 语义"声明的正确构造方式。

下游 morpher 仓库的 `mbase/cli/cli_runner.cpp`（mjt-deobf / mjt-obf 共用的 CLI runner）
在 `validateOutputTarget` / `createStagedOutput` / `publishStagedOutput` 里调用了
`FileUtil::exists` / `create_file` / `move` / `remove`。同仓库的 jar IO
（`mbase/jar/jar_reader.cpp`、`jar_writer.cpp`）已经正确使用了
`std::filesystem::u8path`，但 `FileUtil` 这几个方法仍是裸 `path(std::string)`，
造成同一调用链里一部分正确、一部分有损的不一致状态。

这个坑是 `std::filesystem` API 设计本身的问题（`path(string)` 的编码依赖平台），
应该由 libca 封装层统一屏蔽，而不是让每个下游调用方各自绕过。

## 建议落点

`libca/fs`：

- 主要改动在 `libca/fs/src/libca/fs/file_util.cpp`（实现层）。
- `file_util.hpp` 不需要改签名，但建议在类注释里显式写明"所有 `std::string`
  路径参数按 UTF-8 编码"的契约，强化设计文档已有的声明。

## 设计要点

1. **统一 UTF-8 路径构造**：所有接收 `std::string` 路径参数的 `FileUtil` 方法，
   内部一律用 `std::filesystem::u8path(path)` 构造 `std::filesystem::path`，
   而不是 `std::filesystem::path(path)`。涉及的方法包括但不限于：
   `exists` / `is_file` / `is_directory` / `metadata` / `permissions` /
   `is_readable` / `is_writable` / `list_files` / `list_entries` / `glob` /
   `copy` / `copy_dir` / `move` / `remove` / `remove_all` / `create_file` /
   `create_directories` / `create_temp_file` / `create_temp_directory` /
   `read_all_bytes` / `read_all_text` / `write_bytes` / `write_text` /
   `atomic_write_bytes` / `atomic_write_text` / `read_lines` / `size`。
   实现内部已有的 `open_for_read` / `make_atomic_temp_path` 等辅助函数也要同步改。
2. **Windows `_access` 调用同步**：`is_readable` / `is_writable` 在 Windows 分支
   里用了 `::_access(p.string().c_str(), ...)`，`p.string()` 在 Windows 上会把
   `wchar_t` 路径再按 ACP 编码回 `std::string`，又一次有损。应改为宽字符版
   `::_waccess(p.wstring().c_str(), ...)`，或改用 `std::filesystem::status`
   的权限位判断（与非 Windows 分支一致）。
3. **不改 API 签名**：所有公开方法继续接收 `const std::string&`，保持源码兼容。
   下游（包括 morpher）升级 libca 后无需改调用代码即可获得 Unicode 正确性。
4. **文档化契约**：在 `FileUtil` 类的 Doxygen 注释里加一句"路径参数按 UTF-8 编码"，
   让契约从设计文档延伸到头文件，方便下游阅读。
5. **测试**：在 `libca/fs/unittest/file_util_test.cpp` 增加用例，用包含非 ASCII
   字符的路径（如 `测试目录/中文文件.txt`）覆盖 exists / create_file / move /
   remove / read / write 全链路，证明 Windows 上无有损转换。测试需在 Windows
   非 UTF-8 系统代码页环境下运行才能暴露原 bug，建议在 CI 的 Windows job 上跑。

## 不在范围

- 不规定每个方法精确的 diff；实现者按"统一 u8path + 修 _access"两个原则改即可。
- 不引入 `ca::str::Utf8String` 迁移（设计文档已说明这是独立的后续评估）。
- 不处理 `std::filesystem::path` 在某些 MinGW/旧 MSVC STL 上 `u8path` 的已知
  弃用警告（C++20 起 `u8path` 被标记 deprecated，但直到有等价的
  `std::filesystem::path(std::u8string)` 迁移方案前仍是 Windows 上唯一正确做法；
  实现者可用宏抑制或暂忍警告，本提案不强制）。
- 不改 morpher 侧代码；morpher 升级 libca submodule 指针即可受益。
