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

// ==================== 读写 ====================

Result<ByteVector, std::string> FileUtil::readAllBytes(const std::string& path)
{
    try {
        auto p = std::filesystem::path(path);
        if (!std::filesystem::exists(p)) {
            return Err(std::string("file not found: ") + path);
        }
        if (!std::filesystem::is_regular_file(p)) {
            return Err(std::string("not a regular file: ") + path);
        }

        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return Err(std::string("failed to open file: ") + path);
        }

        auto size = file.tellg();
        if (size < 0) {
            return Err(std::string("failed to determine file size: ") + path);
        }

        ByteVector buffer(static_cast<ca::usize>(size));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        file.close();

        return Ok(std::move(buffer));
    } catch (const std::exception& e) {
        return Err(std::string("readAllBytes failed: ") + e.what());
    }
}

Result<std::string, std::string> FileUtil::readAllText(const std::string& path)
{
    auto result = readAllBytes(path);
    if (result.isErr()) {
        return Err(result.unwrapErr());
    }
    auto bytes = result.unwrap();
    return Ok(std::string(bytes.begin(), bytes.end()));
}

bool FileUtil::writeBytes(const std::string& path, const ByteVector& content, unsigned int mode)
{
    try {
        auto p = std::filesystem::path(path);

        if ((mode & FileMode::CreateNew) && std::filesystem::exists(p)) {
            return false;
        }

        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ios::openmode openMode = std::ios::binary;
        if (mode & FileMode::Append) {
            openMode |= std::ios::app;
        } else {
            openMode |= std::ios::trunc;
        }

        std::ofstream file(p, openMode);
        if (!file.is_open()) return false;

        file.write(reinterpret_cast<const char*>(content.data()),
                   static_cast<std::streamsize>(content.size()));
        file.close();
        return !file.fail();
    } catch (const std::exception&) {
        return false;
    }
}

bool FileUtil::writeText(const std::string& path, const std::string& content, unsigned int mode)
{
    ByteVector bytes(content.begin(), content.end());
    return writeBytes(path, bytes, mode);
}

// ==================== 查询 ====================

ca::i64 FileUtil::getSize(const std::string& path)
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

bool FileUtil::isFile(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
}

bool FileUtil::isDirectory(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(path), ec);
}

// ==================== 遍历 ====================

Result<std::vector<std::string>, std::string> FileUtil::listFiles(const std::string& dir, bool recursive)
{
    try {
        auto p = std::filesystem::path(dir);
        if (!std::filesystem::exists(p))  return Err(std::string("directory not found: ") + dir);
        if (!std::filesystem::is_directory(p)) return Err(std::string("not a directory: ") + dir);

        std::vector<std::string> files;
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(p))
                if (entry.is_regular_file()) files.push_back(entry.path().generic_string());
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(p))
                if (entry.is_regular_file()) files.push_back(entry.path().generic_string());
        }
        return Ok(std::move(files));
    } catch (const std::exception& e) {
        return Err(std::string("listFiles failed: ") + e.what());
    }
}

Result<std::vector<std::string>, std::string> FileUtil::listEntries(const std::string& dir)
{
    try {
        auto p = std::filesystem::path(dir);
        if (!std::filesystem::exists(p))  return Err(std::string("directory not found: ") + dir);
        if (!std::filesystem::is_directory(p)) return Err(std::string("not a directory: ") + dir);

        std::vector<std::string> entries;
        for (const auto& entry : std::filesystem::directory_iterator(p))
            entries.push_back(entry.path().generic_string());
        return Ok(std::move(entries));
    } catch (const std::exception& e) {
        return Err(std::string("listEntries failed: ") + e.what());
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

bool FileUtil::removeAll(const std::string& path)
{
    std::error_code ec;
    auto p = std::filesystem::path(path);
    if (!std::filesystem::exists(p, ec)) return true;  // 不存在视为已删除
    return std::filesystem::remove_all(p, ec) > 0;
}

// ==================== 创建 ====================

bool FileUtil::createFile(const std::string& path)
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

bool FileUtil::createDirectories(const std::string& path)
{
    std::error_code ec;
    auto p = std::filesystem::path(path);
    if (std::filesystem::exists(p, ec)) return true;  // 已存在视为成功
    return std::filesystem::create_directories(p, ec);
}

Result<std::string, std::string> FileUtil::createTempFile(const std::string& prefix,
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
        return Err(std::string("failed to create temp file after 1024 attempts"));
    } catch (const std::exception& e) {
        return Err(std::string("createTempFile failed: ") + e.what());
    }
}

Result<std::string, std::string> FileUtil::createTempDirectory(const std::string& prefix)
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
        return Err(std::string("failed to create temp directory after 1024 attempts"));
    } catch (const std::exception& e) {
        return Err(std::string("createTempDirectory failed: ") + e.what());
    }
}

// ==================== 备份 ====================

Result<std::string, std::string> FileUtil::backup(const std::string& path)
{
    try {
        auto srcPath = std::filesystem::path(path);
        if (!std::filesystem::exists(srcPath)) return Err(std::string("path does not exist: ") + path);

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
            return Err(std::string("unsupported file type: ") + path);
        }
        return Ok(backupPath.generic_string());
    } catch (const std::exception& e) {
        return Err(std::string("backup failed: ") + e.what());
    }
}

// ==================== 权限 ====================

bool FileUtil::isReadable(const std::string& path)
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

bool FileUtil::isWritable(const std::string& path)
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
