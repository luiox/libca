#include "file_util.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <regex>
#include <sstream>
#include <iterator>
#include <stdexcept>

#ifdef _WIN32
#include <io.h>
#include <wchar.h>   // _waccess（宽字符版 access，配合 path::wstring() 无损 Unicode）
#endif

namespace ca { namespace fs {

namespace {

// UTF-8 路径字符串 → 原生 std::filesystem::path 的统一入口（lossy 语义）。
// 本文件不直接使用 std::filesystem::u8path/generic_u8string，编码转换全部经 Path。
std::filesystem::path to_native_path(std::string_view utf8)
{
    return Path::from_utf8_lossy(utf8).native();
}

// 原生 path → UTF-8 字符串（generic 格式，'/' 分隔）的统一出口。
std::string to_utf8_path(const std::filesystem::path& p)
{
    return Path(p).to_utf8_lossy();
}

void remove_if_exists(const std::filesystem::path& path) noexcept
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// read_all_bytes/read_all_text/read_lines 共享的前置流程：校验路径 → 打开文件 → 取字节大小。
// 成功时把已定位到末尾的输入流写入 out_file、字节大小写入 out_size 并返回 FsError::Ok；
// 失败时返回具体 FsError（out_* 未定义）。
// size 上限守卫：文件大小无法用 ca::usize 表示（32 位平台读 >4GB）时返 ReadFailed，
// 防止后续 buffer 分配截断导致缓冲区溢出。
FsError open_for_read(const std::filesystem::path& p, std::ifstream& out_file,
                      std::streamoff& out_size)
{
    if (!std::filesystem::exists(p)) {
        return FsError::FileNotFound;
    }
    if (!std::filesystem::is_regular_file(p)) {
        return FsError::NotARegularFile;
    }

    std::ifstream file(p, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return FsError::OpenFailed;
    }

    auto size = file.tellg();
    if (size < 0) {
        return FsError::ReadFailed;
    }
    // 防御：文件大小超出 usize 可表示范围时拒绝，避免分配截断/缓冲区溢出。
    if (static_cast<unsigned long long>(size) >
        static_cast<unsigned long long>(static_cast<ca::usize>(-1))) {
        return FsError::ReadFailed;
    }

    out_file = std::move(file);
    out_size = size;
    return FsError::Ok;
}

std::filesystem::path make_atomic_temp_path(const std::filesystem::path& dst)
{
    auto parent = dst.has_parent_path() ? dst.parent_path() : std::filesystem::path(".");
    // 临时文件名在 UTF-8 层面拼接（ASCII 前后缀 + 数字），再转回原生 path。
    const std::string name = to_utf8_path(dst.filename());
    // 每个线程只初始化一次随机源，避免 atomic write 高频调用时反复触发系统熵源。
    thread_local static std::mt19937_64 rng(std::random_device{}());
    for (int i = 0; i < 128; ++i) {
        auto candidate = parent / to_native_path(name + ".tmp." + std::to_string(rng()));
        if (!std::filesystem::exists(candidate))
            return candidate;
    }
    return parent / to_native_path(name + ".tmp");
}

bool has_glob_wildcard(const std::string& pattern) noexcept
{
    return pattern.find('*') != std::string::npos ||
           pattern.find('?') != std::string::npos;
}

bool is_regex_meta(char ch) noexcept
{
    switch (ch) {
    case '.':
    case '^':
    case '$':
    case '+':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '|':
    case '\\':
        return true;
    default:
        return false;
    }
}

std::string glob_to_regex(const std::string& pattern)
{
    std::string out = "^";
    for (ca::usize i = 0; i < pattern.size();) {
        const char ch = pattern[i];
        if (ch == '*') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '*') {
                if (i + 2 < pattern.size() && pattern[i + 2] == '/') {
                    out += "(?:.*/)?";
                    i += 3;
                } else {
                    out += ".*";
                    i += 2;
                }
            } else {
                out += "[^/]*";
                ++i;
            }
        } else if (ch == '?') {
            out += "[^/]";
            ++i;
        } else {
            if (is_regex_meta(ch))
                out += '\\';
            out += ch;
            ++i;
        }
    }
    out += "$";
    return out;
}

std::filesystem::path glob_root(const std::string& pattern)
{
    const auto wildcard = pattern.find_first_of("*?");
    if (wildcard == std::string::npos)
        return to_native_path(pattern).parent_path();

    auto prefix = pattern.substr(0, wildcard);
    while (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\')
        prefix.pop_back();
    if (prefix.empty())
        return ".";

    auto root = to_native_path(prefix);
    if (root.has_relative_path() && root.filename().empty())
        root = root.parent_path();
    return root.empty() ? std::filesystem::path(".") : root;
}

bool glob_needs_recursive_walk(const std::string& pattern) noexcept
{
    if (pattern.find("**") != std::string::npos)
        return true;

    const auto first_wildcard = pattern.find_first_of("*?");
    const auto last_slash = pattern.find_last_of('/');
    return first_wildcard != std::string::npos &&
           last_slash != std::string::npos &&
           first_wildcard < last_slash;
}

}  // namespace

// ==================== 读写 ====================

Result<ca::core::Bytes, FsError> FileUtil::read_all_bytes(const std::string& path)
{
    return read_all_bytes(Path::from_utf8_lossy(path));
}

Result<ca::core::Bytes, FsError> FileUtil::read_all_bytes(const Path& path)
{
    try {
        std::ifstream file;
        std::streamoff size = 0;
        if (auto e = open_for_read(path.native(), file, size); e != FsError::Ok) {
            return Err(e);
        }
        auto byte_count = static_cast<ca::usize>(size);

        if (byte_count == 0) {
            return Ok(ca::core::Bytes());
        }

        std::vector<ca::u8> buffer(byte_count);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        if (file.fail() && !file.eof()) {
            return Err(FsError::ReadFailed);
        }
        file.close();

        return Ok(ca::core::Bytes::copy_from_slice(buffer.data(), buffer.size()));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<std::string, FsError> FileUtil::read_all_text(const std::string& path)
{
    return read_all_text(Path::from_utf8_lossy(path));
}

Result<std::string, FsError> FileUtil::read_all_text(const Path& path)
{
    try {
        std::ifstream file;
        std::streamoff size = 0;
        if (auto e = open_for_read(path.native(), file, size); e != FsError::Ok) {
            return Err(e);
        }

        // 直接读入 std::string 并移动返回，避免 vector→Bytes→string 的多重拷贝。
        std::string buffer;
        if (size > 0) {
            buffer.resize(static_cast<std::size_t>(size));
            file.seekg(0, std::ios::beg);
            file.read(&buffer[0], size);
            if (file.fail() && !file.eof()) {
                return Err(FsError::ReadFailed);
            }
        }
        file.close();

        return Ok(std::move(buffer));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<void, FsError> FileUtil::write_bytes(const std::string& path,
                                            const ca::core::ByteSlice& content, unsigned int mode)
{
    return write_bytes(Path::from_utf8_lossy(path), content, mode);
}

Result<void, FsError> FileUtil::write_bytes(const Path& path,
                                            const ca::core::ByteSlice& content, unsigned int mode)
{
    try {
        const auto& p = path.native();

        if ((mode & FileMode::CREATE_NEW) && std::filesystem::exists(p)) {
            return Err(FsError::AlreadyExists);
        }

        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ios::openmode openMode = std::ios::binary;
        if (mode & FileMode::APPEND) {
            openMode |= std::ios::app;
        } else {
            openMode |= std::ios::trunc;
        }

        std::ofstream file(p, openMode);
        if (!file.is_open()) return Err(FsError::OpenFailed);

        file.write(reinterpret_cast<const char*>(content.data()),
                   static_cast<std::streamsize>(content.size()));
        file.close();
        if (file.fail()) return Err(FsError::WriteFailed);
        return Ok();
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<void, FsError> FileUtil::write_text(const std::string& path, const std::string& content,
                                           unsigned int mode)
{
    ca::core::ByteSlice bytes(reinterpret_cast<const ca::u8*>(content.data()), content.size());
    return write_bytes(path, bytes, mode);
}

Result<void, FsError> FileUtil::write_text(const Path& path, const std::string& content,
                                           unsigned int mode)
{
    ca::core::ByteSlice bytes(reinterpret_cast<const ca::u8*>(content.data()), content.size());
    return write_bytes(path, bytes, mode);
}

Result<void, FsError> FileUtil::atomic_write_bytes(const std::string& path,
                                                   const ca::core::ByteSlice& content)
{
    return atomic_write_bytes(Path::from_utf8_lossy(path), content);
}

Result<void, FsError> FileUtil::atomic_write_bytes(const Path& path,
                                                   const ca::core::ByteSlice& content)
{
    const auto& dst = path.native();
    auto tmp = make_atomic_temp_path(dst);

    auto write_result = write_bytes(Path(tmp), content, FileMode::CREATE_NEW);
    if (write_result.is_err()) {
        remove_if_exists(tmp);
        return write_result;
    }

    try {
        std::error_code ec;
        // rename 是唯一的提交点。失败时只清理临时文件，不删除旧目标，保证失败不破坏原数据。
        std::filesystem::rename(tmp, dst, ec);
        if (ec) {
            remove_if_exists(tmp);
            return Err(classify_error(ec));
        }
        return Ok();
    } catch (const std::filesystem::filesystem_error& e) {
        remove_if_exists(tmp);
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        remove_if_exists(tmp);
        return Err(FsError::Unknown);
    }
}

Result<void, FsError> FileUtil::atomic_write_text(const std::string& path,
                                                  const std::string& content)
{
    ca::core::ByteSlice bytes(reinterpret_cast<const ca::u8*>(content.data()), content.size());
    return atomic_write_bytes(path, bytes);
}

Result<void, FsError> FileUtil::atomic_write_text(const Path& path,
                                                  const std::string& content)
{
    ca::core::ByteSlice bytes(reinterpret_cast<const ca::u8*>(content.data()), content.size());
    return atomic_write_bytes(path, bytes);
}

Result<std::vector<std::string>, FsError> FileUtil::read_lines(const std::string& path)
{
    return read_lines(Path::from_utf8_lossy(path));
}

Result<std::vector<std::string>, FsError> FileUtil::read_lines(const Path& path)
{
    try {
        std::ifstream file;
        std::streamoff size = 0;
        if (auto e = open_for_read(path.native(), file, size); e != FsError::Ok)
            return Err(e);

        std::vector<std::string> lines;
        std::string line;
        file.seekg(0, std::ios::beg);
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
        if (file.bad())
            return Err(FsError::ReadFailed);

        return Ok(std::move(lines));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 查询 ====================

ca::i64 FileUtil::size(const std::string& path)
{
    return size(Path::from_utf8_lossy(path));
}

ca::i64 FileUtil::size(const Path& path)
{
    try {
        const auto& p = path.native();
        if (!std::filesystem::exists(p) || !std::filesystem::is_regular_file(p)) return -1;
        return static_cast<ca::i64>(std::filesystem::file_size(p));
    } catch (const std::exception&) {
        return -1;
    }
}

bool FileUtil::exists(const std::string& path)
{
    return exists(Path::from_utf8_lossy(path));
}

bool FileUtil::exists(const Path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path.native(), ec);
}

bool FileUtil::is_file(const std::string& path)
{
    return is_file(Path::from_utf8_lossy(path));
}

bool FileUtil::is_file(const Path& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path.native(), ec);
}

bool FileUtil::is_directory(const std::string& path)
{
    return is_directory(Path::from_utf8_lossy(path));
}

bool FileUtil::is_directory(const Path& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path.native(), ec);
}

Result<FileMetadata, FsError> FileUtil::metadata(const std::string& path)
{
    return metadata(Path::from_utf8_lossy(path));
}

Result<FileMetadata, FsError> FileUtil::metadata(const Path& path)
{
    std::error_code ec;
    const auto& p = path.native();
    auto st = std::filesystem::symlink_status(p, ec);
    if (ec)
        return Err(classify_error(ec));
    if (!std::filesystem::exists(st))
        return Err(FsError::FileNotFound);

    FileMetadata meta;
    meta.exists = true;
    meta.is_file = std::filesystem::is_regular_file(st);
    meta.is_directory = std::filesystem::is_directory(st);
    meta.is_symlink = std::filesystem::is_symlink(st);
    meta.permissions = st.permissions();
    meta.modified_at = std::filesystem::last_write_time(p, ec);
    if (ec)
        return Err(classify_error(ec));
    if (meta.is_file) {
        auto size = std::filesystem::file_size(p, ec);
        if (ec)
            return Err(classify_error(ec));
        meta.size = static_cast<ca::i64>(size);
    }

    return Ok(meta);
}

Result<std::filesystem::perms, FsError> FileUtil::permissions(const std::string& path)
{
    return permissions(Path::from_utf8_lossy(path));
}

Result<std::filesystem::perms, FsError> FileUtil::permissions(const Path& path)
{
    std::error_code ec;
    auto st = std::filesystem::symlink_status(path.native(), ec);
    if (ec)
        return Err(classify_error(ec));
    if (!std::filesystem::exists(st))
        return Err(FsError::FileNotFound);
    return Ok(st.permissions());
}

// ==================== 遍历 ====================

Result<std::vector<std::string>, FsError> FileUtil::list_files(const std::string& dir, bool recursive)
{
    auto files = list_files(Path::from_utf8_lossy(dir), recursive);
    if (files.is_err())
        return Err(std::move(files).unwrap_err());
    auto value = std::move(files).unwrap();
    std::vector<std::string> names;
    names.reserve(value.size());
    for (const auto& f : value)
        names.push_back(f.to_utf8_lossy());
    return Ok(std::move(names));
}

Result<std::vector<Path>, FsError> FileUtil::list_files(const Path& dir, bool recursive)
{
    try {
        const auto& p = dir.native();
        if (!std::filesystem::exists(p))  return Err(FsError::FileNotFound);
        if (!std::filesystem::is_directory(p)) return Err(FsError::NotADirectory);

        std::vector<Path> files;
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(p))
                if (entry.is_regular_file()) files.push_back(Path(entry.path()));
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(p))
                if (entry.is_regular_file()) files.push_back(Path(entry.path()));
        }
        return Ok(std::move(files));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<std::vector<std::string>, FsError> FileUtil::list_entries(const std::string& dir)
{
    auto entries = list_entries(Path::from_utf8_lossy(dir));
    if (entries.is_err())
        return Err(std::move(entries).unwrap_err());
    auto value = std::move(entries).unwrap();
    std::vector<std::string> names;
    names.reserve(value.size());
    for (const auto& e : value)
        names.push_back(e.to_utf8_lossy());
    return Ok(std::move(names));
}

Result<std::vector<Path>, FsError> FileUtil::list_entries(const Path& dir)
{
    try {
        const auto& p = dir.native();
        if (!std::filesystem::exists(p))  return Err(FsError::FileNotFound);
        if (!std::filesystem::is_directory(p)) return Err(FsError::NotADirectory);

        std::vector<Path> entries;
        for (const auto& entry : std::filesystem::directory_iterator(p))
            entries.push_back(Path(entry.path()));
        return Ok(std::move(entries));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 拷贝 / 移动 ====================

bool FileUtil::copy(const std::string& src, const std::string& dst, bool overwrite)
{
    return copy(Path::from_utf8_lossy(src), Path::from_utf8_lossy(dst), overwrite);
}

bool FileUtil::copy(const Path& src, const Path& dst, bool overwrite)
{
    try {
        const auto& srcPath = src.native();
        const auto& dstPath = dst.native();
        if (!std::filesystem::exists(srcPath)) return false;

        auto opts = overwrite
            ? std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive
            : std::filesystem::copy_options::recursive;

        if (dstPath.has_parent_path()) std::filesystem::create_directories(dstPath.parent_path());
        std::filesystem::copy(srcPath, dstPath, opts);
        return true;
    } catch (const std::exception&) { return false; }
}

bool FileUtil::move(const std::string& src, const std::string& dst, bool overwrite)
{
    return move(Path::from_utf8_lossy(src), Path::from_utf8_lossy(dst), overwrite);
}

bool FileUtil::move(const Path& src, const Path& dst, bool overwrite)
{
    try {
        const auto& srcPath = src.native();
        const auto& dstPath = dst.native();
        if (!std::filesystem::exists(srcPath)) return false;

        // 防止 src == dst 时先删除源文件导致数据丢失
        if (std::filesystem::exists(dstPath) &&
            std::filesystem::equivalent(srcPath, dstPath)) {
            return true;
        }

        if (dstPath.has_parent_path()) std::filesystem::create_directories(dstPath.parent_path());
        // 不预删目标：rename 在两平台都会原子替换已存在的普通文件目标
        //（POSIX rename(2)；MSVC 经 MoveFileExW+MOVEFILE_REPLACE_EXISTING）。
        // 此前的 remove_all-then-rename 在 rename 失败时目标已被整棵删光。
        // 目标是已存在目录时 rename 拒绝（返回 false），不再删除目录换移动成功。
        if (!overwrite && std::filesystem::exists(dstPath))
            return false;  // 不覆盖模式：目标存在即失败，不再静默替换
        std::filesystem::rename(srcPath, dstPath);
        return true;
    } catch (const std::exception&) { return false; }
}

Result<void, FsError> FileUtil::copy_dir(const std::string& src, const std::string& dst,
                                         bool overwrite)
{
    return copy_dir(Path::from_utf8_lossy(src), Path::from_utf8_lossy(dst), overwrite);
}

Result<void, FsError> FileUtil::copy_dir(const Path& src, const Path& dst, bool overwrite)
{
    try {
        const auto& srcPath = src.native();
        const auto& dstPath = dst.native();
        if (!std::filesystem::exists(srcPath))
            return Err(FsError::FileNotFound);
        if (!std::filesystem::is_directory(srcPath))
            return Err(FsError::NotADirectory);
        if (std::filesystem::exists(dstPath) && !std::filesystem::is_directory(dstPath)) {
            if (!overwrite)
                return Err(FsError::AlreadyExists);
            std::filesystem::remove(dstPath);
        }

        std::filesystem::create_directories(dstPath);
        auto opts = overwrite
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(srcPath)) {
            auto rel = std::filesystem::relative(entry.path(), srcPath);
            auto target = dstPath / rel;
            if (entry.is_directory()) {
                std::filesystem::create_directories(target);
            } else if (entry.is_regular_file()) {
                if (target.has_parent_path())
                    std::filesystem::create_directories(target.parent_path());
                std::filesystem::copy_file(entry.path(), target, opts);
            } else if (entry.is_symlink()) {
                if (target.has_parent_path())
                    std::filesystem::create_directories(target.parent_path());
                // remove 不跟随符号链接；即使 target 是 broken symlink 也能安全替换。
                if (overwrite)
                    remove_if_exists(target);
                std::filesystem::copy_symlink(entry.path(), target);
            }
        }
        return Ok();
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<std::vector<std::string>, FsError> FileUtil::glob(const std::string& pattern)
{
    try {
        auto normalized = PathUtil::to_unix_separators(pattern);
        if (!has_glob_wildcard(normalized)) {
            auto direct = to_native_path(normalized);
            if (!std::filesystem::exists(direct))
                return Err(FsError::FileNotFound);
            return Ok(std::vector<std::string>{to_utf8_path(direct)});
        }

        auto root = glob_root(normalized);
        if (!std::filesystem::exists(root))
            return Err(FsError::FileNotFound);
        if (!std::filesystem::is_directory(root))
            return Err(FsError::NotADirectory);

        const std::regex matcher(glob_to_regex(normalized), std::regex::ECMAScript);
        std::vector<std::string> matches;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        if (glob_needs_recursive_walk(normalized)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, options)) {
                auto candidate = to_utf8_path(entry.path());
                if (std::regex_match(candidate, matcher))
                    matches.push_back(candidate);
            }
        } else {
            // 普通文件名通配只扫描 root 一层，避免在大目录树上做不必要的递归遍历。
            // 候选 = 模式的目录前缀 + 条目文件名：与模式文本形状严格一致。裸相对
            // 模式（"*.txt" 前缀为空）此前用 directory_iterator(".") 产出的
            // "./a.txt" 候选，多出的 "./" 使正则永不匹配、恒返回空结果。
            const auto  last_slash = normalized.find_last_of('/');
            const std::string prefix =
                (last_slash == std::string::npos) ? std::string()
                                                  : normalized.substr(0, last_slash + 1);
            for (const auto& entry : std::filesystem::directory_iterator(root, options)) {
                auto candidate = prefix + to_utf8_path(entry.path().filename());
                if (std::regex_match(candidate, matcher))
                    matches.push_back(std::move(candidate));
            }
        }
        std::sort(matches.begin(), matches.end());
        return Ok(std::move(matches));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 删除 ====================

bool FileUtil::remove(const std::string& path)
{
    return remove(Path::from_utf8_lossy(path));
}

bool FileUtil::remove(const Path& path)
{
    std::error_code ec;
    return std::filesystem::remove(path.native(), ec);
}

bool FileUtil::remove_all(const std::string& path)
{
    return remove_all(Path::from_utf8_lossy(path));
}

bool FileUtil::remove_all(const Path& path)
{
    std::error_code ec;
    const auto& p = path.native();
    if (!std::filesystem::exists(p, ec)) return true;  // 不存在视为已删除
    return std::filesystem::remove_all(p, ec) > 0;
}

// ==================== 创建 ====================

bool FileUtil::create_file(const std::string& path)
{
    return create_file(Path::from_utf8_lossy(path));
}

bool FileUtil::create_file(const Path& path)
{
    try {
        const auto& p = path.native();
        if (std::filesystem::exists(p)) return false;  // 已有文件不截断
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        std::ofstream file(p, std::ios::binary | std::ios::out);
        if (!file.is_open()) return false;
        file.close();
        return std::filesystem::exists(p);
    } catch (const std::exception&) { return false; }
}

bool FileUtil::create_directories(const std::string& path)
{
    return create_directories(Path::from_utf8_lossy(path));
}

bool FileUtil::create_directories(const Path& path)
{
    std::error_code ec;
    const auto& p = path.native();
    if (std::filesystem::exists(p, ec)) return true;  // 已存在视为成功
    return std::filesystem::create_directories(p, ec);
}

Result<std::string, FsError> FileUtil::create_temp_file(const std::string& prefix,
                                                        const std::string& suffix)
{
    try {
        auto basePath = std::filesystem::absolute(std::filesystem::temp_directory_path());
        std::random_device rd;
        std::mt19937_64 rng(rd());
        for (int i = 0; i < 1024; ++i) {
            auto id = std::to_string(rng());
            auto p = basePath / to_native_path(prefix + id + suffix);
            if (!std::filesystem::exists(p)) {
                std::ofstream f(p, std::ios::binary);
                if (f.is_open()) { f.close(); return Ok(to_utf8_path(p)); }
            }
        }
        return Err(FsError::OpenFailed);
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<std::string, FsError> FileUtil::create_temp_directory(const std::string& prefix)
{
    try {
        auto basePath = std::filesystem::absolute(std::filesystem::temp_directory_path());
        std::random_device rd;
        std::mt19937_64 rng(rd());
        for (int i = 0; i < 1024; ++i) {
            auto id = std::to_string(rng());
            auto p = basePath / to_native_path(prefix + id);
            if (std::filesystem::create_directory(p)) return Ok(to_utf8_path(p));
        }
        return Err(FsError::OpenFailed);
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 备份 ====================

Result<std::string, FsError> FileUtil::backup(const std::string& path)
{
    auto result = backup(Path::from_utf8_lossy(path));
    if (result.is_err())
        return Err(std::move(result).unwrap_err());
    return Ok(std::move(result).unwrap().to_utf8_lossy());
}

Result<Path, FsError> FileUtil::backup(const Path& path)
{
    try {
        const auto& srcPath = path.native();
        if (!std::filesystem::exists(srcPath)) return Err(FsError::FileNotFound);

        auto backupPath = srcPath;
        auto newName = to_utf8_path(srcPath.filename()) + ".backup";
        backupPath = srcPath.has_parent_path()
            ? srcPath.parent_path() / to_native_path(newName)
            : to_native_path(newName);

        if (std::filesystem::is_regular_file(srcPath)) {
            std::filesystem::copy(srcPath, backupPath, std::filesystem::copy_options::overwrite_existing);
        } else if (std::filesystem::is_directory(srcPath)) {
            std::filesystem::copy(srcPath, backupPath,
                std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);
        } else {
            return Err(FsError::NotARegularFile);
        }
        return Ok(Path(backupPath));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 权限 ====================

bool FileUtil::is_readable(const std::string& path)
{
    return is_readable(Path::from_utf8_lossy(path));
}

bool FileUtil::is_readable(const Path& path)
{
    std::error_code ec;
    const auto& p = path.native();
    if (!std::filesystem::exists(p, ec)) return false;

#ifdef _WIN32
    return ::_waccess(p.wstring().c_str(), 4) == 0;
#else
    auto s = std::filesystem::status(p, ec);
    if (ec) return false;
    using P = std::filesystem::perms;
    auto pm = s.permissions();
    return (pm & P::owner_read) != P::none ||
           (pm & P::group_read) != P::none ||
           (pm & P::others_read) != P::none;
#endif
}

bool FileUtil::is_writable(const std::string& path)
{
    return is_writable(Path::from_utf8_lossy(path));
}

bool FileUtil::is_writable(const Path& path)
{
    std::error_code ec;
    const auto& p = path.native();
    if (!std::filesystem::exists(p, ec)) return false;

#ifdef _WIN32
    return ::_waccess(p.wstring().c_str(), 2) == 0;
#else
    auto s = std::filesystem::status(p, ec);
    if (ec) return false;
    using P = std::filesystem::perms;
    auto pm = s.permissions();
    return (pm & P::owner_write) != P::none ||
           (pm & P::group_write) != P::none ||
           (pm & P::others_write) != P::none;
#endif
}

}} // namespace ca::fs
