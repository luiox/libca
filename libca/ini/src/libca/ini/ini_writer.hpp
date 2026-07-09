#pragma once

/// @file ini_writer.hpp
/// @brief INI Writer，把 IniDocument 按行节点顺序写回字符串或文件。

#include "libca/core/result.hpp"
#include "libca/ini/ini_document.hpp"

#include <string>

namespace ca::ini {

/// @brief INI Writer。
class IniWriter {
public:
    /// @brief 序列化 INI 文档。
    /// @param document INI 文档。
    /// @return INI 文本；未修改行会按原始文本输出。
    static std::string write(const IniDocument& document);

    /// @brief 将 INI 文档写入文件。
    /// @param path 文件路径。
    /// @param document INI 文档。
    /// @return 写入成功返回 Ok，打开或写入失败返回错误说明。
    static ca::Result<void, std::string> write_file(
        const std::string& path,
        const IniDocument& document);
};

}  // namespace ca::ini
