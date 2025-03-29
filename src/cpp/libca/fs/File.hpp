#ifndef LIBCA_FS_FILE_HPP
#define LIBCA_FS_FILE_HPP

namespace ca {

// 针对平台的文件操作类封装
class File
{
public:
    File();
    virtual ~File() = default;
    bool open(const char* path, const char* mode);
};


}   // namespace ca

#endif   // !LIBCA_FS_FILE_HPP