#include "path.hpp"

#include <system_error>
#include <utility>

#include "libca/str/utf8_util.hpp"

namespace ca { namespace fs {

// ==================== 构造 / 编码边界 ====================

Result<Path, FsError> Path::from_utf8(std::string_view utf8)
{
    const auto* bytes = reinterpret_cast<const ca::u8*>(utf8.data());
    if (!ca::str::utf8_is_valid(bytes, utf8.size())) {
        return Err(FsError::InvalidUtf8);
    }
    // 已校验合法，lossy 路径不会触发替换，复用同一条转换实现。
    return Ok(from_utf8_lossy(utf8));
}

Path Path::from_utf8_lossy(std::string_view utf8)
{
    return from_os_string(ca::str::OsString::from_utf8_lossy(utf8));
}

Path Path::from_os_string(ca::str::OsString os)
{
#ifdef _WIN32
    // value_type 即 wchar_t，wstring 视图到 path 只是字节搬运，无编码转换。
    return Path(std::filesystem::path(os.as_wide()));
#else
    // POSIX 原生窄编码即 UTF-8，string_view 到 path 为逐字节拷贝。
    return Path(std::filesystem::path(os.as_utf8()));
#endif
}

#ifdef _WIN32
Path Path::from_native(std::wstring_view wide)
{
    return Path(std::filesystem::path(wide));
}
#endif

Path::Path(std::filesystem::path p)
    : path_(std::move(p))
{
}

ca::str::OsString Path::to_os_string() const
{
#ifdef _WIN32
    return ca::str::OsString::from_wstring(path_.native());
#else
    // 目录枚举可能拿到非合法 UTF-8 的字节名，经 lossy 规整为合法 UTF-8。
    return ca::str::OsString::from_utf8_lossy(path_.native());
#endif
}

std::string Path::to_utf8_lossy() const
{
#ifdef _WIN32
    return ca::str::OsString::from_wstring(path_.generic_wstring()).to_utf8_lossy().to_std_string();
#else
    return ca::str::OsString::from_utf8_lossy(path_.generic_string()).to_utf8_lossy().to_std_string();
#endif
}

const std::filesystem::path& Path::native() const noexcept
{
    return path_;
}

// ==================== 组合 / 分解 ====================

Path& Path::operator/=(const Path& tail)
{
    path_ /= tail.path_;
    return *this;
}

Path Path::operator/(const Path& tail) const
{
    Path result(*this);
    result.path_ /= tail.path_;
    return result;
}

Path Path::parent() const
{
    return Path(path_.parent_path());
}

Path Path::filename() const
{
    return Path(path_.filename());
}

Path Path::stem() const
{
    return Path(path_.stem());
}

std::string Path::extension() const
{
    return Path(path_.extension()).to_utf8_lossy();
}

bool Path::is_absolute() const noexcept
{
    return path_.is_absolute();
}

bool Path::is_empty() const noexcept
{
    return path_.empty();
}

Path Path::normalized() const
{
    return Path(path_.lexically_normal());
}

Result<Path, FsError> Path::absolute() const
{
    std::error_code ec;
    auto abs = std::filesystem::absolute(path_, ec);
    if (ec) {
        return Err(classify_error(ec));
    }
    return Ok(Path(std::move(abs)));
}

std::vector<Path> Path::components() const
{
    std::vector<Path> parts;
    for (const auto& part : path_) {
        parts.push_back(Path(part));
    }
    return parts;
}

// ==================== 比较 ====================

bool Path::operator==(const Path& other) const noexcept
{
    return path_ == other.path_;
}

bool Path::operator!=(const Path& other) const noexcept
{
    return path_ != other.path_;
}

bool Path::operator<(const Path& other) const noexcept
{
    return path_ < other.path_;
}

}}  // namespace ca::fs
