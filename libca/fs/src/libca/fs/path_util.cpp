#include "path_util.hpp"

#include <filesystem>
#include <algorithm>

namespace ca { namespace fs {

std::string PathUtil::normalize(const std::string& path)
{
    auto p = std::filesystem::path(path).lexically_normal();
    auto result = p.generic_string();  // 统一使用 '/' 分隔符
    return result;
}

std::string PathUtil::to_unix_separators(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

std::string PathUtil::join(const std::string& base, const std::string& part1)
{
    return (std::filesystem::path(base) /= std::filesystem::path(part1)).generic_string();
}

std::string PathUtil::join(const std::string& base, const std::string& part1, const std::string& part2)
{
    return ((std::filesystem::path(base) /= std::filesystem::path(part1)) /= std::filesystem::path(part2)).generic_string();
}

std::string PathUtil::extension(const std::string& path)
{
    return std::filesystem::path(path).extension().generic_string();
}

std::string PathUtil::stem(const std::string& path)
{
    return std::filesystem::path(path).stem().generic_string();
}

std::string PathUtil::filename(const std::string& path)
{
    return std::filesystem::path(path).filename().generic_string();
}

std::string PathUtil::parent(const std::string& path)
{
    return std::filesystem::path(path).parent_path().generic_string();
}

bool PathUtil::is_absolute(const std::string& path)
{
    return std::filesystem::path(path).is_absolute();
}

std::string PathUtil::to_absolute(const std::string& path)
{
    return std::filesystem::absolute(std::filesystem::path(path)).generic_string();
}

std::vector<std::string> PathUtil::split(const std::string& path)
{
    std::vector<std::string> parts;
    for (const auto& part : std::filesystem::path(path)) {
        parts.push_back(part.generic_string());
    }
    return parts;
}

}}  // namespace ca::fs
