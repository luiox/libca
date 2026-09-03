/// @file path.hpp
/// @brief Path —— 路径值类型：编码边界与路径语义运算。
///        内部持有 std::filesystem::path（Windows 上为 UTF-16，POSIX 上为字节串），
///        编码转换全部经 ca::str::OsString 完成，构造/导出命名与其对齐
///        （from_utf8 / from_utf8_lossy / from_os_string / to_os_string / to_utf8_lossy）。
///        语义对齐 Rust std::path::Path：可拷贝值语义，operator/ 拼接
///        （右操作数为绝对路径时整体替换），native 词法比较（不访问文件系统）。

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "fs_error.hpp"
#include "libca/core/result.hpp"
#include "libca/str/os_string.hpp"

namespace ca { namespace fs {

/// 路径值类型
///
/// 与 std::string 之间不做隐式转换：裸字符串不携带编码信息，从字符串构造必须经
/// from_utf8 / from_utf8_lossy 工厂显式表达编码语义（from_utf8 校验并报
/// FsError::InvalidUtf8，from_utf8_lossy 用 U+FFFD 替代非法序列、绝不失败）。
///
/// @note to_utf8_lossy 输出统一 '/' 分隔符（generic 格式）；to_os_string 输出
///       平台原生格式（Windows 反斜杠）。Windows 文件名含未配对代理等无法映射
///       到合法 UTF-8 的字符时，lossy 导出按 U+FFFD 替代（from_native 可无损承载）。
class Path
{
public:
    Path() noexcept = default;
    ~Path() = default;
    Path(const Path&) = default;
    Path& operator=(const Path&) = default;
    Path(Path&&) noexcept = default;
    Path& operator=(Path&&) noexcept = default;

    // ==================== 构造 / 编码边界 ====================

    /// @brief 从 UTF-8 字符串构造，显式校验合法性。
    /// @param utf8 UTF-8 编码的路径。
    /// @return 合法返回 Ok；含非法 UTF-8 序列返回 Err(FsError::InvalidUtf8)。
    static Result<Path, FsError> from_utf8(std::string_view utf8);

    /// @brief 从 UTF-8 字符串构造，非法序列以 U+FFFD 替代，绝不失败。
    /// @param utf8 UTF-8 编码的路径。
    /// @return Path；与 from_utf8 的关系对标 ca::str::OsString::from_utf8_lossy。
    static Path from_utf8_lossy(std::string_view utf8);

    /// @brief 从平台原生编码字符串构造（Windows 上不做编码转换）。
    /// @param os 原生编码字符串（Windows UTF-16 / POSIX UTF-8）。
    static Path from_os_string(ca::str::OsString os);

#ifdef _WIN32
    /// @brief 从宽字符路径构造（Windows 专用）。
    /// @note 不做任何校验与转换，保留全部信息——包括未配对代理等无法映射到
    ///       合法 UTF-8 的名字，用于 Win32 API 边界的无损互操作。
    static Path from_native(std::wstring_view wide);
#endif

    /// @brief 接管 std::filesystem::path，与标准库互操作。
    explicit Path(std::filesystem::path p);

    /// @brief 导出为平台原生编码（native 格式，Windows 反斜杠分隔）。
    ca::str::OsString to_os_string() const;

    /// @brief 导出为 UTF-8 字符串（generic 格式，'/' 分隔）。
    /// @note 无法映射到合法 UTF-8 的字符按 U+FFFD 替代（lossy 语义）。
    std::string to_utf8_lossy() const;

    /// @brief 底层 std::filesystem::path 只读引用。
    /// @note 可直接传给 std::filesystem API 与 fstream 构造（C++17 起原生支持）。
    const std::filesystem::path& native() const noexcept;

    // ==================== 组合 / 分解 ====================

    /// @brief 追加路径段。tail 为绝对路径时整体替换为 tail（std::filesystem 语义）。
    Path& operator/=(const Path& tail);

    /// @brief 拼接产生新 Path，不改自身。
    Path operator/(const Path& tail) const;

    /// @brief 父目录路径。
    /// @note 根路径的父是自身；无父（相对单段）时返回空 Path。
    Path parent() const;

    /// @brief 文件名部分（含扩展名）。
    /// @note 路径以分隔符结尾或为根时返回空 Path。
    Path filename() const;

    /// @brief 无扩展名的文件名部分。
    Path stem() const;

    /// @brief 扩展名（含 '.'，如 ".txt"）；无扩展名返回空字符串。
    std::string extension() const;

    /// 是否为绝对路径。
    bool is_absolute() const noexcept;

    /// 是否为空路径。
    bool is_empty() const noexcept;

    /// @brief 词法归一化（消除冗余 . 与 ..），不访问文件系统。
    Path normalized() const;

    /// @brief 基于当前工作目录转为绝对路径。
    /// @return 成功返回 Ok；获取失败（如工作目录不可得）返回 FsError。
    Result<Path, FsError> absolute() const;

    /// @brief 按分隔符拆为组件（"a/b/c" → [a, b, c]）。
    /// @note 根（POSIX "/"、Windows 盘符与 UNC 前缀）单独成组件。
    std::vector<Path> components() const;

    // ==================== 比较 ====================

    /// @brief native 词法相等（大小写敏感；'/' 与 '\\' 在 Windows 上视为相同分隔符）。
    bool operator==(const Path& other) const noexcept;

    /// native 词法不等。
    bool operator!=(const Path& other) const noexcept;

    /// native 词法序，可作关联容器 key。
    bool operator<(const Path& other) const noexcept;

private:
    std::filesystem::path path_;
};

}}  // namespace ca::fs

/// 支持 std::hash，可用作 unordered 容器 key。
namespace std {
template <>
struct hash<ca::fs::Path>
{
    size_t operator()(const ca::fs::Path& p) const noexcept
    {
        return hash<std::filesystem::path>()(p.native());
    }
};
}  // namespace std
