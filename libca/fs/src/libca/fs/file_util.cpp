#include "file_util.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <iterator>
#include <stdexcept>

#ifdef _WIN32
#include <io.h>
#endif

namespace ca { namespace fs {

namespace {

/// 把 std::error_code 归类为 FsError。无错误映射时退回 Unknown。
FsError classify_fs_error(const std::error_code& ec) noexcept
{
    if (!ec) return FsError::Unknown;
    // 优先用 std::errc 条件值判断，跨平台稳定。
    const auto cond = static_cast<std::errc>(ec.default_error_condition().value());
    switch (cond) {
        case std::errc::no_such_file_or_directory:    return FsError::FileNotFound;
        case std::errc::not_a_directory:              return FsError::NotADirectory;
        case std::errc::permission_denied:            return FsError::PermissionDenied;
        case std::errc::file_exists:                  return FsError::AlreadyExists;
        case std::errc::no_space_on_device:           return FsError::DiskFull;
        case std::errc::read_only_file_system:        return FsError::PermissionDenied;
        case std::errc::no_buffer_space:              return FsError::DiskFull;
        default:                                       return FsError::Unknown;
    }
}

}  // namespace

// ==================== 读写 ====================

Result<ca::core::Bytes, FsError> FileUtil::read_all_bytes(const std::string& path)
{
    try {
        auto p = std::filesystem::path(path);
        if (!std::filesystem::exists(p)) {
            return Err(FsError::FileNotFound);
        }
        if (!std::filesystem::is_regular_file(p)) {
            return Err(FsError::NotARegularFile);
        }

        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return Err(FsError::OpenFailed);
        }

        auto size = file.tellg();
        if (size < 0) {
            return Err(FsError::ReadFailed);
        }

        std::vector<ca::u8> buffer(static_cast<ca::usize>(size));
        file.seekg(0, std::ios::beg);
        if (size > 0) {
            file.read(reinterpret_cast<char*>(buffer.data()), size);
            if (file.fail() && !file.eof()) {
                return Err(FsError::ReadFailed);
            }
        }
        file.close();

        return Ok(ca::core::Bytes::copy_from_slice(buffer.data(), buffer.size()));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_fs_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<std::string, FsError> FileUtil::read_all_text(const std::string& path)
{
    auto result = read_all_bytes(path);
    if (result.is_err()) {
        return Err(result.unwrap_err());
    }
    auto bytes = result.unwrap();
    const ca::u8* ptr = bytes.as_ptr();
    return Ok(std::string(reinterpret_cast<const char*>(ptr), bytes.len()));
}

Result<void, FsError> FileUtil::write_bytes(const std::string& path,
                                            const ca::core::ByteSlice& content, unsigned int mode)
{
    try {
        auto p = std::filesystem::path(path);

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
        return Err(classify_fs_error(e.code()));
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

// ==================== 查询 ====================

ca::i64 FileUtil::size(const std::string& path)
{
    try {
        auto p = std::filesystem::path(path);
        if (!std::filesystem::exists(p) || !std::filesystem::is_regular_file(p)) return -1;
        return static_cast<ca::i64>(std::filesystem::file_size(p));
    } catch (const std::exception&) {
        return -1;
    }
}

bool FileUtil::exists(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

bool FileUtil::is_file(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
}

bool FileUtil::is_directory(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(path), ec);
}

// ==================== 遍历 ====================

Result<std::vector<std::string>, FsError> FileUtil::list_files(const std::string& dir, bool recursive)
{
    try {
        auto p = std::filesystem::path(dir);
        if (!std::filesystem::exists(p))  return Err(FsError::FileNotFound);
        if (!std::filesystem::is_directory(p)) return Err(FsError::NotADirectory);

        std::vector<std::string> files;
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(p))
                if (entry.is_regular_file()) files.push_back(entry.path().generic_string());
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(p))
                if (entry.is_regular_file()) files.push_back(entry.path().generic_string());
        }
        return Ok(std::move(files));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_fs_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

Result<std::vector<std::string>, FsError> FileUtil::list_entries(const std::string& dir)
{
    try {
        auto p = std::filesystem::path(dir);
        if (!std::filesystem::exists(p))  return Err(FsError::FileNotFound);
        if (!std::filesystem::is_directory(p)) return Err(FsError::NotADirectory);

        std::vector<std::string> entries;
        for (const auto& entry : std::filesystem::directory_iterator(p))
            entries.push_back(entry.path().generic_string());
        return Ok(std::move(entries));
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_fs_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 拷贝 / 移动 ====================

bool FileUtil::copy(const std::string& src, const std::string& dst, bool overwrite)
{
    try {
        auto srcPath = std::filesystem::path(src);
        auto dstPath = std::filesystem::path(dst);
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
    try {
        auto srcPath = std::filesystem::path(src);
        auto dstPath = std::filesystem::path(dst);
        if (!std::filesystem::exists(srcPath)) return false;

        // 防止 src == dst 时先删除源文件导致数据丢失
        if (std::filesystem::exists(dstPath) &&
            std::filesystem::equivalent(srcPath, dstPath)) {
            return true;
        }

        if (dstPath.has_parent_path()) std::filesystem::create_directories(dstPath.parent_path());
        if (overwrite && std::filesystem::exists(dstPath)) std::filesystem::remove_all(dstPath);
        std::filesystem::rename(srcPath, dstPath);
        return true;
    } catch (const std::exception&) { return false; }
}

// ==================== 删除 ====================

bool FileUtil::remove(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::remove(std::filesystem::path(path), ec);
}

bool FileUtil::remove_all(const std::string& path)
{
    std::error_code ec;
    auto p = std::filesystem::path(path);
    if (!std::filesystem::exists(p, ec)) return true;  // 不存在视为已删除
    return std::filesystem::remove_all(p, ec) > 0;
}

// ==================== 创建 ====================

bool FileUtil::create_file(const std::string& path)
{
    try {
        auto p = std::filesystem::path(path);
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
    std::error_code ec;
    auto p = std::filesystem::path(path);
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
            auto p = basePath / (prefix + id + suffix);
            if (!std::filesystem::exists(p)) {
                std::ofstream f(p, std::ios::binary);
                if (f.is_open()) { f.close(); return Ok(p.generic_string()); }
            }
        }
        return Err(FsError::OpenFailed);
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_fs_error(e.code()));
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
            auto p = basePath / (prefix + id);
            if (std::filesystem::create_directory(p)) return Ok(p.generic_string());
        }
        return Err(FsError::OpenFailed);
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_fs_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 备份 ====================

Result<std::string, FsError> FileUtil::backup(const std::string& path)
{
    try {
        auto srcPath = std::filesystem::path(path);
        if (!std::filesystem::exists(srcPath)) return Err(FsError::FileNotFound);

        auto backupPath = srcPath;
        auto newName = srcPath.filename().generic_string() + ".backup";
        backupPath = srcPath.has_parent_path()
            ? srcPath.parent_path() / newName
            : std::filesystem::path(newName);

        if (std::filesystem::is_regular_file(srcPath)) {
            std::filesystem::copy(srcPath, backupPath, std::filesystem::copy_options::overwrite_existing);
        } else if (std::filesystem::is_directory(srcPath)) {
            std::filesystem::copy(srcPath, backupPath,
                std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);
        } else {
            return Err(FsError::NotARegularFile);
        }
        return Ok(backupPath.generic_string());
    } catch (const std::filesystem::filesystem_error& e) {
        return Err(classify_fs_error(e.code()));
    } catch (const std::exception&) {
        return Err(FsError::Unknown);
    }
}

// ==================== 权限 ====================

bool FileUtil::is_readable(const std::string& path)
{
    std::error_code ec;
    auto p = std::filesystem::path(path);
    if (!std::filesystem::exists(p, ec)) return false;

#ifdef _WIN32
    return ::_access(p.string().c_str(), 4) == 0;
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
    std::error_code ec;
    auto p = std::filesystem::path(path);
    if (!std::filesystem::exists(p, ec)) return false;

#ifdef _WIN32
    return ::_access(p.string().c_str(), 2) == 0;
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
