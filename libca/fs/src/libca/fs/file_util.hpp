#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "path_util.hpp"
#include <libca/core/result.hpp>
#include <libca/core/datatype.hpp>

namespace ca { namespace fs {

/// 文件写入模式常量
struct FileMode
{
    static constexpr unsigned int OVERWRITE   = 0x01;  ///< 覆盖写入（默认）
    static constexpr unsigned int APPEND      = 0x02;  ///< 追加写入
    static constexpr unsigned int CREATE_NEW  = 0x04;  ///< 创建新文件，失败若已存在
};

using ByteVector = std::vector<ca::u8>;

/// 文件与目录操作工具类
///
/// 封装 std::filesystem，提供类似 Java FileUtil 的便捷静态接口。
/// 可能失败的操作返回 ca::Result<T, std::string>，纯查询返回裸值。
class FileUtil
{
public:
    // ==================== 读写 ====================

    /// 读取整个文件为字节数组
    static Result<ByteVector, std::string> read_all_bytes(const std::string& path);

    /// 读取整个文件为 UTF-8 字符串
    static Result<std::string, std::string> read_all_text(const std::string& path);

    /// 按模式写入字节数组到文件
    static bool write_bytes(const std::string& path, const ByteVector& content,
                            unsigned int mode = FileMode::OVERWRITE);

    /// 按模式写入字符串到文件
    static bool write_text(const std::string& path, const std::string& content,
                           unsigned int mode = FileMode::OVERWRITE);

    // ==================== 查询 ====================

    /// 获取文件大小（字节）。文件不存在或出错返回 -1。
    static ca::i64 size(const std::string& path);

    /// 判断路径是否存在
    static bool exists(const std::string& path);

    /// 判断是否为普通文件
    static bool is_file(const std::string& path);

    /// 判断是否为目录
    static bool is_directory(const std::string& path);

    // ==================== 遍历 ====================

    /// 列出目录下所有文件（不含子目录本身）。recursive=true 时递归所有子目录。
    static Result<std::vector<std::string>, std::string> list_files(const std::string& dir,
                                                                     bool recursive = false);

    /// 列出目录下的直接条目（文件和子目录）
    static Result<std::vector<std::string>, std::string> list_entries(const std::string& dir);

    // ==================== 拷贝 / 移动 ====================

    /// 拷贝文件或目录。overwrite=true 时覆盖已存在的目标。
    static bool copy(const std::string& src, const std::string& dst, bool overwrite = true);

    /// 移动（重命名）文件或目录。overwrite=true 时覆盖已存在的目标。
    static bool move(const std::string& src, const std::string& dst, bool overwrite = true);

    // ==================== 删除 ====================

    /// 删除文件或空目录
    static bool remove(const std::string& path);

    /// 递归删除文件或目录（无论是否为空）
    static bool remove_all(const std::string& path);

    // ==================== 创建 ====================

    /// 创建空文件（自动创建父目录）
    static bool create_file(const std::string& path);

    /// 递归创建目录
    static bool create_directories(const std::string& path);

    /// 创建临时文件，返回完整路径
    static Result<std::string, std::string> create_temp_file(const std::string& prefix = "",
                                                              const std::string& suffix = "");

    /// 创建临时目录，返回完整路径
    static Result<std::string, std::string> create_temp_directory(const std::string& prefix = "");

    // ==================== 备份 ====================

    /// 备份文件或目录，追加 ".backup" 后缀。返回备份路径。
    static Result<std::string, std::string> backup(const std::string& path);

    // ==================== 权限 ====================

    /// 判断文件或目录是否可读
    static bool is_readable(const std::string& path);

    /// 判断文件或目录是否可写
    static bool is_writable(const std::string& path);
};

}}  // namespace ca::fs
