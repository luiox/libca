#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fs_error.hpp"
#include "path.hpp"
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

/// @brief 文件元数据快照。
/// @note 使用 symlink_status 获取类型信息，因此 is_symlink 可以识别符号链接本身。
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
///
/// 每个接受路径的函数提供两个重载族：
/// - **std::string 族**（兼容存量代码）：路径一律按 **UTF-8** 编码，内部经
///   `Path::from_utf8_lossy` 转换——非法 UTF-8 序列以 U+FFFD 替代，语义确定、不抛异常。
/// - **Path 族**（推荐）：类型化编码边界，无隐式字符串转换；返回路径的重载族
///   相应返回 Path（如 list_files 返回 `std::vector<Path>`）。需要严格校验非法
///   UTF-8 时，先用 `Path::from_utf8`（失败返回 FsError::InvalidUtf8）构造。
///
/// glob 与 create_temp_file/create_temp_directory 的参数是模式/前后缀而非路径，
/// 仅提供 std::string 版本。
class FileUtil
{
public:
    // ==================== 读写 ====================

    /// 读取整个文件为不可变字节序列 ca::core::Bytes（自管理生命周期）。
    static Result<ca::core::Bytes, FsError> read_all_bytes(const std::string& path);

    /// @copydoc read_all_bytes(const std::string&)
    static Result<ca::core::Bytes, FsError> read_all_bytes(const Path& path);

    /// 读取整个文件为 UTF-8 字符串
    static Result<std::string, FsError> read_all_text(const std::string& path);

    /// @copydoc read_all_text(const std::string&)
    static Result<std::string, FsError> read_all_text(const Path& path);

    /// 按模式写入字节序列到文件。content 为非拥有只读视图（ca::core::ByteSlice）。
    static Result<void, FsError> write_bytes(const std::string& path,
                                             const ca::core::ByteSlice& content,
                                             unsigned int mode = FileMode::OVERWRITE);

    /// @copydoc write_bytes(const std::string&, const ca::core::ByteSlice&, unsigned int)
    static Result<void, FsError> write_bytes(const Path& path,
                                             const ca::core::ByteSlice& content,
                                             unsigned int mode = FileMode::OVERWRITE);

    /// 按模式写入字符串到文件
    static Result<void, FsError> write_text(const std::string& path, const std::string& content,
                                            unsigned int mode = FileMode::OVERWRITE);

    /// @copydoc write_text(const std::string&, const std::string&, unsigned int)
    static Result<void, FsError> write_text(const Path& path, const std::string& content,
                                            unsigned int mode = FileMode::OVERWRITE);

    /// @brief 原子写入字节：先写入同目录临时文件，再用 rename 提交。
    /// @param path 目标文件路径。
    /// @param content 待写入的非拥有字节视图。
    /// @return 成功返回 Ok；失败返回 FsError，并尽力清理临时文件。
    /// @note 失败时不会主动删除或截断既有目标文件；rename 是唯一提交点。
    static Result<void, FsError> atomic_write_bytes(const std::string& path,
                                                    const ca::core::ByteSlice& content);

    /// @copydoc atomic_write_bytes(const std::string&, const ca::core::ByteSlice&)
    static Result<void, FsError> atomic_write_bytes(const Path& path,
                                                    const ca::core::ByteSlice& content);

    /// @brief 原子写入 UTF-8 文本：先写入同目录临时文件，再用 rename 提交。
    /// @param path 目标文件路径。
    /// @param content 待写入文本，按原始字节写入，不做编码转换。
    /// @return 成功返回 Ok；失败返回 FsError，并尽力清理临时文件。
    /// @note 语义同 atomic_write_bytes，适合配置、缓存、状态文件等需要失败不破坏旧内容的场景。
    static Result<void, FsError> atomic_write_text(const std::string& path,
                                                   const std::string& content);

    /// @copydoc atomic_write_text(const std::string&, const std::string&)
    static Result<void, FsError> atomic_write_text(const Path& path,
                                                   const std::string& content);

    /// @brief 按行读取文本文件，支持 LF/CRLF。
    /// @param path 文本文件路径。
    /// @return 返回行数组；行尾换行符不包含在返回值中，CRLF 会去掉末尾 CR。
    static Result<std::vector<std::string>, FsError> read_lines(const std::string& path);

    /// @copydoc read_lines(const std::string&)
    static Result<std::vector<std::string>, FsError> read_lines(const Path& path);

    // ==================== 查询 ====================

    /// 获取文件大小（字节）。文件不存在或出错返回 -1。
    static ca::i64 size(const std::string& path);

    /// @copydoc size(const std::string&)
    static ca::i64 size(const Path& path);

    /// 判断路径是否存在
    static bool exists(const std::string& path);

    /// @copydoc exists(const std::string&)
    static bool exists(const Path& path);

    /// 判断是否为普通文件
    static bool is_file(const std::string& path);

    /// @copydoc is_file(const std::string&)
    static bool is_file(const Path& path);

    /// 判断是否为目录
    static bool is_directory(const std::string& path);

    /// @copydoc is_directory(const std::string&)
    static bool is_directory(const Path& path);

    /// @brief 获取路径元数据。
    /// @param path 文件系统路径。
    /// @return 成功返回 FileMetadata；路径不存在或查询失败返回 FsError。
    static Result<FileMetadata, FsError> metadata(const std::string& path);

    /// @copydoc metadata(const std::string&)
    static Result<FileMetadata, FsError> metadata(const Path& path);

    /// @brief 获取路径权限位。
    /// @param path 文件系统路径。
    /// @return 成功返回 std::filesystem::perms；路径不存在或查询失败返回 FsError。
    static Result<std::filesystem::perms, FsError> permissions(const std::string& path);

    /// @copydoc permissions(const std::string&)
    static Result<std::filesystem::perms, FsError> permissions(const Path& path);

    // ==================== 遍历 ====================

    /// 列出目录下所有文件（不含子目录本身）。recursive=true 时递归所有子目录。
    static Result<std::vector<std::string>, FsError> list_files(const std::string& dir,
                                                                 bool recursive = false);

    /// @copydoc list_files(const std::string&, bool)
    /// @note 返回 Path 列表，避免每条结果再做一次 UTF-8 编码转换。
    static Result<std::vector<Path>, FsError> list_files(const Path& dir, bool recursive = false);

    /// 列出目录下的直接条目（文件和子目录）
    static Result<std::vector<std::string>, FsError> list_entries(const std::string& dir);

    /// @copydoc list_entries(const std::string&)
    /// @note 返回 Path 列表，避免每条结果再做一次 UTF-8 编码转换。
    static Result<std::vector<Path>, FsError> list_entries(const Path& dir);

    // ==================== 拷贝 / 移动 ====================

    /// 拷贝文件或目录。overwrite=true 时覆盖已存在的目标。
    static bool copy(const std::string& src, const std::string& dst, bool overwrite = true);

    /// @copydoc copy(const std::string&, const std::string&, bool)
    static bool copy(const Path& src, const Path& dst, bool overwrite = true);

    /// 移动（重命名）文件或目录。overwrite=true 时覆盖已存在的目标。
    static bool move(const std::string& src, const std::string& dst, bool overwrite = true);

    /// @copydoc move(const std::string&, const std::string&, bool)
    static bool move(const Path& src, const Path& dst, bool overwrite = true);

    /// @brief 递归拷贝目录。
    /// @param src 源目录路径，必须存在且为目录。
    /// @param dst 目标目录路径，不存在时自动创建。
    /// @param overwrite true 时覆盖已存在的文件或符号链接目标；false 时遇到冲突返回错误。
    /// @return 成功返回 Ok；失败返回 FsError。
    /// @note 符号链接按链接本身复制，不跟随链接内容；覆盖 broken symlink 时会先移除链接本身。
    static Result<void, FsError> copy_dir(const std::string& src, const std::string& dst,
                                          bool overwrite = true);

    /// @copydoc copy_dir(const std::string&, const std::string&, bool)
    static Result<void, FsError> copy_dir(const Path& src, const Path& dst,
                                          bool overwrite = true);

    /// @brief 文件系统 glob 匹配。
    /// @param pattern 匹配模式（UTF-8），使用 `/` 作为逻辑分隔符；Windows 路径会归一化后匹配。
    /// @return 成功返回按字典序排序的匹配路径（UTF-8，'/' 分隔）；根目录不存在或不是目录时返回 FsError。
    /// @note 支持 `*`、`?`、`**`。仅文件名含通配时只扫描一层；包含 `**` 或目录段通配时递归扫描。
    static Result<std::vector<std::string>, FsError> glob(const std::string& pattern);

    // ==================== 删除 ====================

    /// 删除文件或空目录
    static bool remove(const std::string& path);

    /// @copydoc remove(const std::string&)
    static bool remove(const Path& path);

    /// 递归删除文件或目录（无论是否为空）
    static bool remove_all(const std::string& path);

    /// @copydoc remove_all(const std::string&)
    static bool remove_all(const Path& path);

    // ==================== 创建 ====================

    /// 创建空文件（自动创建父目录）
    static bool create_file(const std::string& path);

    /// @copydoc create_file(const std::string&)
    static bool create_file(const Path& path);

    /// 递归创建目录
    static bool create_directories(const std::string& path);

    /// @copydoc create_directories(const std::string&)
    static bool create_directories(const Path& path);

    /// 创建临时文件，返回完整路径（UTF-8）。prefix/suffix 为文件名前后缀（UTF-8），非路径。
    static Result<std::string, FsError> create_temp_file(const std::string& prefix = "",
                                                          const std::string& suffix = "");

    /// 创建临时目录，返回完整路径（UTF-8）。prefix 为目录名前缀（UTF-8），非路径。
    static Result<std::string, FsError> create_temp_directory(const std::string& prefix = "");

    // ==================== 备份 ====================

    /// 备份文件或目录，追加 ".backup" 后缀。返回备份路径。
    static Result<std::string, FsError> backup(const std::string& path);

    /// @copydoc backup(const std::string&)
    /// @note 返回 Path，避免编码往返。
    static Result<Path, FsError> backup(const Path& path);

    // ==================== 权限 ====================

    /// 判断文件或目录是否可读
    static bool is_readable(const std::string& path);

    /// @copydoc is_readable(const std::string&)
    static bool is_readable(const Path& path);

    /// 判断文件或目录是否可写
    static bool is_writable(const std::string& path);

    /// @copydoc is_writable(const std::string&)
    static bool is_writable(const Path& path);
};

}}  // namespace ca::fs
