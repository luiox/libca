#include "File.hpp"
#include <cstring>

namespace ca {
// 检查路径是否合法

bool FileSystem::isVaildPath(const String& path) {
    auto len = path.length();
    if (len == 0 || len > MaxPathLen) {
        return false;
    }
    for (auto i = 0; i < strlen(IllgealChars); i++) {
        for (auto j = 0; j < len; j++) {
            if (path[j] == IllgealChars[i]) {
                return false;
            }
        }
    }
    return true;

}
// 从字符串创建Path对象
FileSystemItem FileSystemItem::of(const char* path)
{
    // 检查路径是否合法
    if (!FileSystem::isValidPath(path)) {
        return FileSystemItem(ItemType::File ,".");
    }
    return FileSystemItem(ItemType::Directory,path);
}

File::File(String& path)
{

}

bool File::open(const char* path, const char* mode)
{
    return false;
}

}   // namespace ca
