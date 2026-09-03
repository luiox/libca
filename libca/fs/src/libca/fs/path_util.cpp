#include "path_util.hpp"

#include <algorithm>

#include "path.hpp"

namespace ca { namespace fs {

// 本文件不直接使用 std::filesystem::u8path/generic_u8string：UTF-8 编码语义统一经
// Path 的 from_utf8_lossy / to_utf8_lossy 边界，转换实现全库只有 path.cpp 一份。

std::string PathUtil::normalize(const std::string& path)
{
    // 先把反斜杠归一为 '/'：POSIX 上 '\\' 不是分隔符，归一化前后语义保持与本类历史行为一致。
    return Path::from_utf8_lossy(to_unix_separators(path)).normalized().to_utf8_lossy();
}

std::string PathUtil::to_unix_separators(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

std::string PathUtil::join(const std::string& base, const std::string& part1)
{
    return (Path::from_utf8_lossy(base) / Path::from_utf8_lossy(part1)).to_utf8_lossy();
}

std::string PathUtil::join(const std::string& base, const std::string& part1, const std::string& part2)
{
    return ((Path::from_utf8_lossy(base) / Path::from_utf8_lossy(part1)) /
            Path::from_utf8_lossy(part2))
        .to_utf8_lossy();
}

std::string PathUtil::extension(const std::string& path)
{
    return Path::from_utf8_lossy(path).extension();
}

std::string PathUtil::stem(const std::string& path)
{
    return Path::from_utf8_lossy(path).stem().to_utf8_lossy();
}

std::string PathUtil::filename(const std::string& path)
{
    return Path::from_utf8_lossy(path).filename().to_utf8_lossy();
}

std::string PathUtil::parent(const std::string& path)
{
    return Path::from_utf8_lossy(path).parent().to_utf8_lossy();
}

bool PathUtil::is_absolute(const std::string& path)
{
    return Path::from_utf8_lossy(path).is_absolute();
}

std::string PathUtil::to_absolute(const std::string& path)
{
    // 此前直接调用抛异常版 std::filesystem::absolute，极端失败时异常会逃逸出本类
    //（与“所有方法均不抛异常”的承诺不符）；现失败时返回原路径。
    auto result = Path::from_utf8_lossy(path).absolute();
    return result.is_ok() ? result.unwrap().to_utf8_lossy() : path;
}

std::vector<std::string> PathUtil::split(const std::string& path)
{
    std::vector<std::string> parts;
    for (const auto& part : Path::from_utf8_lossy(path).components()) {
        parts.push_back(part.to_utf8_lossy());
    }
    return parts;
}

}}  // namespace ca::fs
