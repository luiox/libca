#include <libca/log/level.hpp>
#include <map>
#include <string>

namespace libca::log {
// 使用 std::map 存储枚举值到字符串的映射
std::map<Level, std::string> levelToStringMap = {{Level::Unknown, "Unknown"},
                                                 {Level::Debug, "Debug"},
                                                 {Level::Info, "Info"},
                                                 {Level::Warn, "Warn"},
                                                 {Level::Error, "Error"},
                                                 {Level::Fatal, "Fatal"}};
// 从字符串转换为日志级别
Level stringToLevel(const std::string& level)
{
    for (auto& it : levelToStringMap) {
        if (it.second == level) {
            return it.first;
        }
    }
    // 如果找不到，返回 Unknown 级别
    return Level::Unknown;
}

// 将日志级别转换为字符串
std::string levelToString(Level level)
{
    auto it = levelToStringMap.find(level);
    if (it != levelToStringMap.end()) {
        return it->second;
    }
    // 如果没有找到，返回一个Unknown的字符串
    return std::string("Unknown");
}
}   // namespace libca::log