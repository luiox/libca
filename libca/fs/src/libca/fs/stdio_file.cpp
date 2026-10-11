#include "stdio_file.hpp"

#include <cerrno>
#include <cstring>
#include <system_error>

namespace ca { namespace fs {

namespace {

// fseek/ftell 的 64 位口径：MSVC 下裸 fseek/ftell 是 32 位（>2GB 文件截断）。
#ifdef _WIN32
#define LIBCA_FS_SEEK64 _fseeki64
#define LIBCA_FS_TELL64 _ftelli64
#else
#define LIBCA_FS_SEEK64 fseeko
#define LIBCA_FS_TELL64 ftello
#endif

// 打开失败的错误归类：fopen/_wfopen 失败即设置 errno，经 classify_error 映射为
// FsError（ENOENT→FileNotFound、EACCES→PermissionDenied、EMFILE→TooManyOpenFiles 等）。
FsError last_errno_error() noexcept
{
    return classify_error(std::error_code(errno, std::generic_category()));
}

}  // namespace

Result<StdioFile, FsError> open_stdio(const Path& path, const char* mode)
{
#ifdef _WIN32
    // mode 仅含 ASCII（"rb"/"wb" 等），逐字符拓宽即可（与 zip::open_stdio_file 同口径）。
    std::wstring wide_mode;
    for (const char* q = mode; *q != '\0'; ++q)
        wide_mode.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*q)));

    const auto& native = path.native();
#if defined(_MSC_VER)
    std::FILE* fp = nullptr;
    if (::_wfopen_s(&fp, native.c_str(), wide_mode.c_str()) != 0)
        return Err(last_errno_error());
#else
    std::FILE* fp = ::_wfopen(native.c_str(), wide_mode.c_str());
    if (fp == nullptr)
        return Err(last_errno_error());
#endif
    return Ok(StdioFile(fp));
#else
    std::FILE* fp = std::fopen(path.native().c_str(), mode);
    if (fp == nullptr)
        return Err(last_errno_error());
    return Ok(StdioFile(fp));
#endif
}

Result<std::string, FsError> read_file_all(const Path& path)
{
    auto opened = open_stdio(path, "rb");
    if (opened.is_err())
        return Err(std::move(opened).unwrap_err());
    StdioFile file = std::move(opened).unwrap();

    if (LIBCA_FS_SEEK64(file.get(), 0, SEEK_END) != 0)
        return Err(last_errno_error());
    const long long size = LIBCA_FS_TELL64(file.get());
    if (size < 0)
        return Err(last_errno_error());
    // 防御：文件大小超出 usize 可表示范围时拒绝，避免分配截断/缓冲区溢出
    //（与 FileUtil 读路径同口径，32 位平台读 >4GB）。
    if (static_cast<unsigned long long>(size) >
        static_cast<unsigned long long>(static_cast<ca::usize>(-1))) {
        return Err(FsError::ReadFailed);
    }
    std::rewind(file.get());

    std::string buffer(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        // 预清 errno：成功调用不清 errno，避免陈旧值被误归类为本次读取失败的原因。
        errno = 0;
        const std::size_t done =
            std::fread(&buffer[0], 1, static_cast<std::size_t>(size), file.get());
        if (done != static_cast<std::size_t>(size)) {
            if (std::ferror(file.get()) != 0 && errno != 0)
                return Err(last_errno_error());  // 如 POSIX 目录 EISDIR→IsADirectory
            return Err(FsError::ReadFailed);
        }
    }
    return Ok(std::move(buffer));
}

}}  // namespace ca::fs
