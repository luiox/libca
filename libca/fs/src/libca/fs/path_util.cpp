#include "path_util.hpp"

#include <filesystem>
#include <algorithm>
#include <cctype>

namespace ca { namespace fs {

std::string PathUtil::normalize(const std::string& path)
{
    auto p = std::filesystem::u8path(to_unix_separators(path)).lexically_normal();
    auto result = p.generic_u8string();  // 统一使用 '/' 分隔符
    return result;
}

Result<std::string, FsError> PathUtil::normalize_within(const std::string& path,
                                                        const std::string& base)
{
    if (path.empty() || base.empty())
        return Err(FsError::PathOutsideBase);

    // 纯词法处理：反斜杠先统一为 '/'，再各自 lexically_normal（消除 . 与 .. 段），
    // 全程不访问文件系统。
    auto base_p = std::filesystem::u8path(to_unix_separators(base)).lexically_normal();
    auto path_p = std::filesystem::u8path(to_unix_separators(path)).lexically_normal();

    // 根一致性检查一：root_name 有无不一致（如一方带盘符/UNC、另一方是纯相对路径）。
    if (path_p.has_root_name() != base_p.has_root_name())
        return Err(FsError::PathOutsideBase);
    if (path_p.has_root_name()) {
        const auto path_root = path_p.root_name().generic_u8string();
        const auto base_root = base_p.root_name().generic_u8string();
        // Windows 盘符大小写不区分（c:/ 与 C:/ 同一根）；盘符不同则必然不在 base 之下。
        // 部分标准库实现的 lexically_relative 不做盘符大小写折叠，这里显式比较：
        // 仅大小写差异时用 base 的书写形式做相对化，结果仍保留 path 原书写形式。
        bool root_differs = path_root.size() != base_root.size();
        for (std::size_t i = 0; !root_differs && i < path_root.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(path_root[i])) !=
                std::tolower(static_cast<unsigned char>(base_root[i])))
                root_differs = true;
        }
        if (root_differs)
            return Err(FsError::PathOutsideBase);
    }

    // 根一致性检查二：root_directory 有无不一致。注意 Windows 上 "/x" 无盘符不算
    // is_absolute，但实际解析到当前驱动器根，与相对 base 不在同一层级，必须拒绝。
    if (path_p.has_root_directory() != base_p.has_root_directory())
        return Err(FsError::PathOutsideBase);

    // 盘符仅大小写差异时（前面已验证忽略大小写相等），在 relative_path 层面做相对化：
    // 绕开 operator/ 的 drive-relative 拼接语义（path("C:")/"x" 得到 "C:x"）与部分
    // 标准库 lexically_relative 不折叠盘符大小写的实现差异。
    std::filesystem::path relative;
    if (path_p.has_root_name() && path_p.root_name() != base_p.root_name())
        relative = path_p.relative_path().lexically_relative(base_p.relative_path());
    else
        relative = path_p.lexically_relative(base_p);
    if (relative.empty())
        return Err(FsError::PathOutsideBase);

    // 相对化结果以 ".." 开头说明词法上已逃逸出 base；rel == "." 表示 path 归一化后
    // 等于 base 本身（含边界，允许）。
    if (*relative.begin() == "..")
        return Err(FsError::PathOutsideBase);

    return Ok(path_p.generic_u8string());
}

std::string PathUtil::to_unix_separators(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

std::string PathUtil::join(const std::string& base, const std::string& part1)
{
    return (std::filesystem::u8path(base) /= std::filesystem::u8path(part1)).generic_u8string();
}

std::string PathUtil::join(const std::string& base, const std::string& part1, const std::string& part2)
{
    return ((std::filesystem::u8path(base) /= std::filesystem::u8path(part1)) /= std::filesystem::u8path(part2)).generic_u8string();
}

std::string PathUtil::extension(const std::string& path)
{
    return std::filesystem::u8path(path).extension().generic_u8string();
}

std::string PathUtil::stem(const std::string& path)
{
    return std::filesystem::u8path(path).stem().generic_u8string();
}

std::string PathUtil::filename(const std::string& path)
{
    return std::filesystem::u8path(path).filename().generic_u8string();
}

std::string PathUtil::parent(const std::string& path)
{
    return std::filesystem::u8path(path).parent_path().generic_u8string();
}

bool PathUtil::is_absolute(const std::string& path)
{
    return std::filesystem::u8path(path).is_absolute();
}

std::string PathUtil::to_absolute(const std::string& path)
{
    return std::filesystem::absolute(std::filesystem::u8path(path)).generic_u8string();
}

std::vector<std::string> PathUtil::split(const std::string& path)
{
    std::vector<std::string> parts;
    for (const auto& part : std::filesystem::u8path(path)) {
        parts.push_back(part.generic_u8string());
    }
    return parts;
}

}}  // namespace ca::fs
