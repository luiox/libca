#pragma once

/// @file yaml_reader.hpp
/// @brief YAML Reader：字符串/文件 → YamlDocument 的静态入口。
/// @details 内部驱动 YamlParser。所有字符串 intern 进返回的 document 的 arena，
///          document 是整个 DOM 的所有权根。

#include "libca/core/result.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/yaml/parse_error.hpp"
#include "libca/yaml/yaml_document.hpp"

namespace ca::yaml {

/// @brief YAML 读取选项（配置子集固定语义，目前无可配项，保留扩展位）。
struct YamlReaderOptions {};

/// @brief YAML Reader，静态入口。
class YamlReader {
public:
    /// @brief 解析 YAML 文本。
    /// @param input 输入视图（零拷贝，使用期内须有效）。
    /// @return 成功返回持有 DOM 的 YamlDocument；失败返回首个 ParseError。
    static ca::Result<YamlDocument, ParseError> read(
        const ca::str::Utf8StringRef& input,
        const YamlReaderOptions& options = YamlReaderOptions());

    /// @brief 读取并解析 YAML 文件。
    /// @param path 文件路径（UTF-8）。
    /// @return 成功返回 YamlDocument；打开失败或解析失败返回 ParseError。
    static ca::Result<YamlDocument, ParseError> read_file(
        const ca::str::Utf8StringRef& path,
        const YamlReaderOptions& options = YamlReaderOptions());
};

}  // namespace ca::yaml
