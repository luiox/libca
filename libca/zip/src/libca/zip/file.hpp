#pragma once

#include <memory>
#include <string>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"
#include "libca/zip/entry.hpp"
#include "libca/zip/zip_error.hpp"

namespace ca::zip {

/// @brief 面向 JVM ZipFile 语义的只读 ZIP 访问器。
///
/// 解析行为对齐 java.util.zip.ZipFile：以最后一条合法 EOCD 为准，支持
/// 前缀拼接（自提取类）与 ZIP64。常规失败（文件不存在、结构损坏、CRC 不符、
/// 条目缺失等）经 Result 返回；异常仅保留在打开型构造重载（失败抛
/// std::runtime_error）与 get_entry_at 的越界编程错误（抛 std::out_of_range）。
class ZipFile {
public:
    ZipFile();

    /// @brief 打开磁盘上的归档并立即解析。
    /// @note 构造路径保留异常：失败抛 std::runtime_error；需要 Result 语义请
    ///       先默认构造再调 open(path)。
    explicit ZipFile(const std::string& path);

    /// @brief 从内存镜像解析归档。
    /// @note 构造路径保留异常：解析失败抛 std::runtime_error；需要 Result
    ///       语义请先默认构造再调 open(data)。
    explicit ZipFile(const std::vector<ca::u8>& data);

    ~ZipFile();

    /// @brief 打开磁盘归档并解析；失败时保持关闭状态。
    Result<void, ZipErrorInfo> open(const std::string& path);

    /// @brief 从内存镜像解析归档；失败时保持关闭状态。
    Result<void, ZipErrorInfo> open(const std::vector<ca::u8>& data);

    bool is_open() const;
    void close();

    /// @brief 条目数。
    size_t size() const;

    /// @brief 按名字精确查找条目；不存在返回 nullptr。
    const ZipEntry* get_entry(const std::string& name) const;

    /// @brief 按序号取条目；越界抛 std::out_of_range（编程错误，对齐 std::vector::at）。
    const ZipEntry& get_entry_at(size_t index) const;

    /// @brief 全部条目名（按 CEN 出现顺序）。
    std::vector<std::string> entries() const;

    /// @brief 读取并解压条目内容。
    /// @note 条目缺失返回 NOT_FOUND；CRC 校验失败返回 CRC_MISMATCH；
    ///       未支持的压缩方法返回 UNSUPPORTED。
    Result<std::vector<ca::u8>, ZipErrorInfo> read(const std::string& name) const;

    /// @brief 同上，直接指定条目。
    Result<std::vector<ca::u8>, ZipErrorInfo> read(const ZipEntry& entry) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::zip
