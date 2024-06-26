#ifndef LIBCA_LOG_LEVEL_H
#define LIBCA_LOG_LEVEL_H

#include <string>

namespace libca::log
{
    // 日志等级
    enum class Level { Unknown, Debug, Info, Warn, Error, Fatal };

    // 从字符串转换为日志级别
    Level stringToLevel(const std::string & level);

    // 将日志级别转换为字符串
    std::string levelToString(Level level);

}

#endif // !LIBCA_LOG_LEVEL_H
