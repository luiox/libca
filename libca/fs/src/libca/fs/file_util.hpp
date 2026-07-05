#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fs_error.hpp"
#include "path_util.hpp"
#include "libca/core/bytes.hpp"
#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

namespace ca { namespace fs {

/// 文件写入模式常量
struct FileMode
{
    static constexpr unsigned int OVERWRITE   = 0x01;  ///< 覆盖写入（默认）
    static constexpr unsigned int APPEND      = 0x02;  ///< 追加写入
    static constexpr unsigned int CREATE_NEW  = 0x04;  ///< 创建新文件，失败若已存在
};

/// 文件元数据快照。
struct FileMetadata
{
    bool exists = false;                         ///< 路径是否存在
    bool is_file = false;                        ///< 是否为普通文件
    bool is_directory = false;                   ///< 是否为目录
    bool is_symlink = false;                     ///< 是否为符号链接
    ca::i64 size = -1;                           ///< 普通文件大小；非普通文件为 -1
    std::filesystem::perms permissions{};        ///< 文件权限位
    std::filesystem::file_time_type modified_at; ///< 最后修改时间
};

/// 文件与目录操作工具类
///
/// 封装 std::filesystem，提供类似 Java FileUtil 的便捷静态接口。
/// 可能失败且需知原因的操作返回 ca::Result<T, FsError>（结构化错误码，可用 to_string 转可读字符串）；
/// 纯查询/只关心成败的操作返回裸 bool 或哨兵值。不向外抛异常。
class FileUtil
{
public:
    // ==================== 读写 ====================

    /// 读取整个文件为不可变字节序列 ca::core::Bytes（自管理生命周期）。
    static Result<ca::core::Bytes, FsError> read_all_bytes(const std::string& path);

    /// 读取整个文件为 UTF-8 字符串
    static Result<std::string, FsError> read_all_text(const std::string& path);

    /// 按模式写入字节序列到文件。content 为非拥有只读视图（ca::core::ByteSlice）。
    static Result<void, FsError> write_bytes(const std::string& path,
                                             const ca::core::ByteSlice& content,
                                             unsigned int mode = FileMode::OVERWRITE);

    /// 按模式写入字符串到文件
    static Result<void, FsError> write_text(const std::string& path, const std::string& content,
                                            unsigned int mode = FileMode::OVERWRITE);

    /// 原子写入字节：先写入同目录临时文件，再替换目标路径。
    static Result<void, FsError> atomic_write_bytes(const std::string& path,
                                                    const ca::core::ByteSlice& content);

    /// 原子写入文本：先写入同目录临时文件，再替换目标路径。
    static Result<void, FsError> atomic_write_text(const std::string& path,
                                                   const std::string& content);

    /// 按行读取文本文件，支持 LF/CRLF。行尾换行符不包含在返回值中。
    static Result<std::vector<std::string>, FsError> read_lines(const std::string& path);

    // ==================== 查询 ====================

    /// 获取文件大小（字节）。文件不存在或出错返回 -1。
    static ca::i64 size(const std::string& path);

    /// 判断路径是否存在
    static bool exists(const std::string& path);

    /// 判断是否为普通文件
    static bool is_file(const std::string& path);

    /// 判断是否为目录
    static bool is_directory(const std::string& path);

    /// 获取路径元数据。
    static Result<FileMetadata, FsError> metadata(const std::string& path);

    /// 获取路径权限位。
    static Result<std::filesystem::perms, FsError> permissions(const std::string& path);

    // ==================== 遍历 ====================

    /// 列出目录下所有文件（不含子目录本身）。recursive=true 时递归所有子目录。
    static Result<std::vector<std::string>, FsError> list_files(const std::string& dir,
                                                                 bool recursive = false);

    /// 列出目录下的直接条目（文件和子目录）
    static Result<std::vector<std::string>, FsError> list_entries(const std::string& dir);

    // ==================== 拷贝 / 移动 ====================

    /// 拷贝文件或目录。overwrite=true 时覆盖已存在的目标。
    static bool copy(const std::string& src, const std::string& dst, bool overwrite = true);

    /// 移动（重命名）文件或目录。overwrite=true 时覆盖已存在的目标。
    static bool move(const std::string& src, const std::string& dst, bool overwrite = true);

    /// 递归拷贝目录。overwrite=true 时覆盖已存在的文件。
    static Result<void, FsError> copy_dir(const std::string& src, const std::string& dst,
                                          bool overwrite = true);

    /// 文件系统 glob。支持 *、?、** 通配，返回匹配路径。
    static Result<std::vector<std::string>, FsError> glob(const std::string& pattern);

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
    static Result<std::string, FsError> create_temp_file(const std::string& prefix = "",
                                                          const std::string& suffix = "");

    /// 创建临时目录，返回完整路径
    static Result<std::string, FsError> create_temp_directory(const std::string& prefix = "");

    // ==================== 备份 ====================

    /// 备份文件或目录，追加 ".backup" 后缀。返回备份路径。
    static Result<std::string, FsError> backup(const std::string& path);

    // ==================== 权限 ====================

    /// 判断文件或目录是否可读
    static bool is_readable(const std::string& path);

    /// 判断文件或目录是否可写
    static bool is_writable(const std::string& path);
};

}}  // namespace ca::fs
