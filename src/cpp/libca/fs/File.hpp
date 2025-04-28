#pragma once

#include "libca/base/ByteBuffer.hpp"
#include "libca/base/String.hpp"
#include "libca/base/Result.hpp"
#include <string>

namespace ca {

class Path
{
private:
    std::string path;

    bool isIllegalChar(char c)
    {
        for (size_t i = 0; IllgealChars[i] != '\0'; ++i) {
            if (c == IllgealChars[i]) {
                return true;
            }
        }
        return false;
    }

    bool isSeparator(char c) { return c == Seprator; }

public:
    constexpr static const char* IllgealChars = "\\/:*?\"<>|";
    constexpr static size_t      MaxPathLen   = 8;
#if CA_PLATFORM_WINDOWS
    constexpr static char Seprator = '\\';
#else
    constexpr static char Seprator = '/';
#endif

    static bool isValidPath(const String& path);

    static Result<Path, std::string> of(std::string path);
    Path(const std::string& p)
        : path(p)
    {}

    bool isFile() const
    {
#if CA_PLATFORM_WINDOWS
        auto attrs = GetFileAttributesA(path.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat statbuf;
        if (stat(path.c_str(), &statbuf) != 0) {
            return false;
        }
        return S_ISREG(statbuf.st_mode);
#endif
    }

    bool isDirectory() const
    {
#if CA_PLATFORM_WINDOWS
        auto attrs = GetFileAttributesA(path.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat statbuf;
        if (stat(path.c_str(), &statbuf) != 0) {
            return false;
        }
        return S_ISDIR(statbuf.st_mode);
#endif
    }

    void normalize()
    {
        std::string normalized;
        for (size_t i = 0; i < path.length(); ++i) {
            if (!isIllegalChar(path[i])) {
                if (isSeparator(path[i])) {
                    if (normalized.empty() || !isSeparator(normalized.back())) {
                        normalized.push_back(Seprator);
                    }
                }
                else {
                    normalized.push_back(path[i]);
                }
            }
        }
        path = normalized;
    }

    void append(const std::string& part)
    {
        if (!part.empty()) {
            if (!path.empty() && !isSeparator(path.back())) {
                path.push_back(Seprator);
            }
            path += part;
            normalize();
        }
    }

    std::string toString() const { return path; }
};

// 文件系统的项，可以是文件或者目录
class FileSystemItem final
{
public:
    enum ItemType
    {
        Directory,
        File
    };

private:
    ItemType type_;
    String   paht_;

    FileSystemItem(ItemType type, const char* path);

public:
    static FileSystemItem of(const char* path);

    bool isDirectory() const;

    bool isFile() const;

    bool mkdir() const;
};

// 针对平台的文件IO操作类封装
class File final
{
    String path_;

public:
    File(String& path);
    ~File();

    bool exists() const;

    bool rename(const char* path) const;
    bool deleteFile() const;

    bool open(const char* path, const char* mode);
    bool close();

    usize read(void* buf, usize size);
    usize write(const void* buf, usize size);
    bool  seek(usize offset, i32 whence);
    usize tell() const;
    usize size() const;

    static Result<ByteBuffer, String> readAllBytes(String filePath);
};

class FileSystem final
{
public:
    
    static bool exists(const String& path);

    static String getCurrentDirectory();
    static String getTempDirectory();
    static String getHomeDirectory();
};

}   // namespace ca
