#pragma once

#include <string>
#include <vector>

#include "fs_error.hpp"
#include "libca/core/result.hpp"

namespace ca { namespace fs {

/// 路径字符串操作工具类
///
/// 路径解析与拼接运算，内部基于 `std::filesystem::path`；除 `to_absolute` 外不访问
/// 文件系统。所有方法均不抛异常。
///
/// @note 所有 `std::string` 路径参数一律按 **UTF-8** 编码，内部统一用
///       `std::filesystem::u8path` 构造路径，避免 Windows 上按本地代码页解析导致
///       中文/非 ASCII 路径有损。
class PathUtil
{
public:
    /// 统一路径分隔符为 '/'，并进行词法归一化（去除冗余的 . 和 .. 段）
    static std::string normalize(const std::string& path);

    /// @brief 词法规范化 path 并检查是否仍位于 base 之内（含 base 本身），路径穿越防护。
    ///
    /// 先对 path 与 base 各自做 normalize（统一 '/'、消除冗余的 . 与 .. 段），再要求
    /// path 的归一化结果位于 base 的归一化结果之下。**纯词法层面**：不解析符号链接、
    /// 不访问文件系统，因此不能替代操作系统层面的权限/符号链接校验。
    ///
    /// @param path 待检查的路径。
    /// @param base 允许活动的根目录。
    /// @return 成功返回 path 规范化后的路径（保留原书写形式，如盘符大小写）；失败返回
    ///         FsError::PathOutsideBase，包括：归一化后逃逸出 base、绝对路径注入、
    ///         path 与 base 根不匹配（不同盘符、绝对/相对混用）、空 path 或空 base。
    /// @note path 归一化后等于 base 视为在 base 之内（含边界），返回规范化后的 base。
    /// @note Windows 反斜杠按 normalize 语义统一为 '/'；盘符大小写按平台 root_name
    ///       语义比较（不区分大小写），目录段按词法精确比较（对大小写不敏感文件系统
    ///       采取保守策略：宁可误拒、不可误放）。
    static Result<std::string, FsError> normalize_within(const std::string& path,
                                                         const std::string& base);

    /// 仅将路径中的反斜杠替换为斜杠，不做其他归一化
    static std::string to_unix_separators(const std::string& path);

    /// 拼接两个路径段，自动处理分隔符
    static std::string join(const std::string& base, const std::string& part1);

    /// 拼接三个路径段
    static std::string join(const std::string& base, const std::string& part1, const std::string& part2);

    /// 获取文件扩展名（含 '.'，如 ".txt"），无扩展名返回空字符串
    static std::string extension(const std::string& path);

    /// 获取无扩展名的文件名部分
    static std::string stem(const std::string& path);

    /// 获取文件名（含扩展名）
    static std::string filename(const std::string& path);

    /// 获取父目录路径。对根路径返回自身，对相对路径 "." 返回 ".."
    static std::string parent(const std::string& path);

    /// 判断路径是否为绝对路径
    static bool is_absolute(const std::string& path);

    /// 转为绝对路径（基于当前工作目录进行解析）
    static std::string to_absolute(const std::string& path);

    /// 拆分路径为各段。如 "a/b/c" -> ["a", "b", "c"]
    static std::vector<std::string> split(const std::string& path);
};

}}  // namespace ca::fs
