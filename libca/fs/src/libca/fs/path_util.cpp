#include "path_util.hpp"

#include <algorithm>
#include <cctype>

#include "path.hpp"

namespace ca { namespace fs {

// 本文件不直接使用 std::filesystem::u8path/generic_u8string：UTF-8 编码语义统一经
// Path 的 from_utf8_lossy / to_utf8_lossy 边界，转换实现全库只有 path.cpp 一份。

std::string PathUtil::normalize(const std::string& path)
{
    // 先把反斜杠归一为 '/'：POSIX 上 '\\' 不是分隔符，归一化前后语义保持与本类历史行为一致。
    return Path::from_utf8_lossy(to_unix_separators(path)).normalized().to_utf8_lossy();
}

Result<std::string, FsError> PathUtil::normalize_within(const std::string& path,
                                                        const std::string& base)
{
    if (path.empty() || base.empty())
        return Err(FsError::PathOutsideBase);

    // 纯词法处理：反斜杠先统一为 '/'，再各自 lexically_normal（消除 . 与 .. 段），
    // 全程不访问文件系统。
    auto base_p = Path::from_utf8_lossy(to_unix_separators(base)).native().lexically_normal();
    auto path_p = Path::from_utf8_lossy(to_unix_separators(path)).native().lexically_normal();

    // 根一致性检查一：root_name 有无不一致（如一方带盘符/UNC、另一方是纯相对路径）。
    if (path_p.has_root_name() != base_p.has_root_name())
        return Err(FsError::PathOutsideBase);
    if (path_p.has_root_name()) {
        const auto path_root = Path(path_p.root_name()).to_utf8_lossy();
        const auto base_root = Path(base_p.root_name()).to_utf8_lossy();
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

    return Ok(Path(path_p).to_utf8_lossy());
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
