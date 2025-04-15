#pragma once

#include "Path.hpp"

namespace ca {

// 针对平台的文件操作类封装
class File final
{
    Path path_;
public:
    File();
    ~File();
    
    bool isFile() const;
    bool isDirectory() const;

    bool exists() const;
    bool mkdir() const;

    static File getCurrentDirectory();
    static File getTempDirectory();
    static File getHomeDirectory();

    bool open(const char* path, const char* mode);
    bool close();

    bool read(void* buf, size_t size);
    bool write(const void* buf, size_t size);
    bool seek(size_t offset, int whence);
    size_t tell() const;
    size_t size() const;
    bool truncate(size_t size);

};

}   // namespace ca
