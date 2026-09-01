#pragma once

#include <memory>
#include <string>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/zip/entry.hpp"

namespace ca::zip {

/// @brief 面向 JVM ZipFile 语义的只读 ZIP 访问器。
///
/// 解析行为对齐 java.util.zip.ZipFile：以最后一条合法 EOCD 为准，支持
/// 前缀拼接（自提取类）与 ZIP64。错误时抛 std::runtime_error。
class ZipFile
{
public:
    ZipFile();

    /// @brief 打开磁盘上的归档并立即解析。
    explicit ZipFile(const std::string& path);

    /// @brief 从内存镜像解析归档。
    explicit ZipFile(const std::vector<ca::u8>& data);

    ~ZipFile();

    void open(const std::string& path);
    void open(const std::vector<ca::u8>& data);
    bool is_open() const;
    void close();

    /// @brief 条目数。
    size_t size() const;

    /// @brief 按名字精确查找条目；不存在返回 nullptr。
    const ZipEntry* get_entry(const std::string& name) const;

    /// @brief 按序号取条目；越界抛 std::out_of_range。
    const ZipEntry& get_entry_at(size_t index) const;

    /// @brief 全部条目名（按 CEN 出现顺序）。
    std::vector<std::string> entries() const;

    /// @brief 读取并解压条目内容；CRC 校验失败或条目缺失抛异常。
    std::vector<ca::u8> read(const std::string& name) const;
    std::vector<ca::u8> read(const ZipEntry& entry) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::zip
