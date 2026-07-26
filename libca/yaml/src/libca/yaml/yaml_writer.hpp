#pragma once

/// @file yaml_writer.hpp
/// @brief YAML 序列化器：YamlWriter。把 YamlDocument 写为 Utf8String。
/// @details 块式输出（非空集合一律块式，空集合用 flow []/{}）。字符串按需加引号——
///          若不加引号会被读成非字符串类型（如 "true"/"3.14"/"~"）则强制加引号，
///          保证 write→read 类型保真。含换行的字符串输出为 | 字面块标量。

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/yaml/yaml_document.hpp"

namespace ca::yaml {

/// @brief YAML 序列化选项。
struct YamlWriterOptions {
    /// 每层缩进空格数。
    ca::usize indent = 2;
};

/// @brief YAML 序列化器。
class YamlWriter {
public:
    /// @brief 把 YamlDocument 序列化为 Utf8String。
    static ca::str::Utf8String write(
        const YamlDocument& document,
        const YamlWriterOptions& options = YamlWriterOptions());

    /// @brief 把 YamlDocument 写入文件。
    /// @return 成功返回 Ok；写失败返回错误说明 Utf8String。
    static ca::Result<void, ca::str::Utf8String> write_file(
        const ca::str::Utf8StringRef& path,
        const YamlDocument& document,
        const YamlWriterOptions& options = YamlWriterOptions());
};

}  // namespace ca::yaml
