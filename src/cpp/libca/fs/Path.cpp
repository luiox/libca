#include "Path.hpp"
#include "libca/base/Platform.hpp"

namespace ca {

Path::Path(const char* path)
    : path_(path)
{
    // 已经限制必须从of创建Path对象了，所以构造函数里面的一定是合法的
}

// 检查路径是否合法
bool Path::isValid(const char* path)
{
    auto len = strlen(path);
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
Path Path::of(const char* path)
{
    // 检查路径是否合法
    if (!isValid(path)) {
        return Path(".");
    }
    return Path(path);
}

}   // namespace ca
