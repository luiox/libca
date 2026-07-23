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
/// @note 所有 `std::string` 路径参数一律按 **UTF-8** 编码。内部统一用
///       `std::filesystem::u8path` 构造路径，在 Windows 上正确按 UTF-8 解码为
///       原生 wchar_t 路径，避免 `std::filesystem::path(std::string)` 按本地代码页
///       （ACP）解析导致中文/非 ASCII 路径有损。
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

    /// @brief 原子写入字节：先写入同目录临时文件，再用 rename 提交。
    /// @param path 目标文件路径。
    /// @param content 待写入的非拥有字节视图。
    /// @return 成功返回 Ok；失败返回 FsError，并尽力清理临时文件。
    /// @note 失败时不会主动删除或截断既有目标文件；rename 是唯一提交点。
    static Result<void, FsError> atomic_write_bytes(const std::string& path,
                                                    const ca::core::ByteSlice& content);

    /// @brief 原子写入 UTF-8 文本：先写入同目录临时文件，再用 rename 提交。
    /// @param path 目标文件路径。
    /// @param content 待写入文本，按原始字节写入，不做编码转换。
    /// @return 成功返回 Ok；失败返回 FsError，并尽力清理临时文件。
    /// @note 语义同 atomic_write_bytes，适合配置、缓存、状态文件等需要失败不破坏旧内容的场景。
    static Result<void, FsError> atomic_write_text(const std::string& path,
                                                   const std::string& content);

    /// @brief 按行读取文本文件，支持 LF/CRLF。
    /// @param path 文本文件路径。
    /// @return 成功返回行数组；行尾换行符不包含在返回值中，CRLF 会去掉末尾 CR。
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

    /// @brief 获取路径元数据。
    /// @param path 文件系统路径。
    /// @return 成功返回 FileMetadata；路径不存在或查询失败返回 FsError。
    static Result<FileMetadata, FsError> metadata(const std::string& path);

    /// @brief 获取路径权限位。
    /// @param path 文件系统路径。
    /// @return 成功返回 std::filesystem::perms；路径不存在或查询失败返回 FsError。
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

    /// @brief 递归拷贝目录。
    /// @param src 源目录路径，必须存在且为目录。
    /// @param dst 目标目录路径，不存在时自动创建。
    /// @param overwrite true 时覆盖已存在的文件或符号链接目标；false 时遇到冲突返回错误。
    /// @return 成功返回 Ok；失败返回 FsError。
    /// @note 符号链接按链接本身复制，不跟随链接内容；覆盖 broken symlink 时会先移除链接本身。
    static Result<void, FsError> copy_dir(const std::string& src, const std::string& dst,
                                          bool overwrite = true);

    /// @brief 文件系统 glob 匹配。
    /// @param pattern 匹配模式，使用 `/` 作为逻辑分隔符；Windows 路径会归一化后匹配。
    /// @return 成功返回按字典序排序的匹配路径；根目录不存在或不是目录时返回 FsError。
    /// @note 支持 `*`、`?`、`**`。仅文件名含通配时只扫描一层；包含 `**` 或目录段通配时递归扫描。
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
