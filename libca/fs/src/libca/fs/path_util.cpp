#include "path_util.hpp"

#include <filesystem>
#include <algorithm>

namespace ca {
namespace fs {

std::string PathUtil::normalize(const std::string& path)
{
    auto p      = std::filesystem::u8path(to_unix_separators(path)).lexically_normal();
    auto result = p.generic_u8string();   // 统一使用 '/' 分隔符
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
    return (std::filesystem::u8path(base) /= std::filesystem::u8path(part1)).generic_u8string();
}

std::string PathUtil::join(const std::string& base, const std::string& part1,
                           const std::string& part2)
{
    return ((std::filesystem::u8path(base) /= std::filesystem::u8path(part1)) /=
            std::filesystem::u8path(part2))
        .generic_u8string();
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

}   // namespace fs
}   // namespace ca
