#include "libca/log/logger_registry.hpp"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ca::log {

namespace {

// Meyers 单例：局部 static 保证首次访问初始化线程安全。
// 持有 shared_ptr<Logger> 而非裸指针；map 是唯一活跃持有者，因此替换/注销/
// clear 时把旧 Logger 移入退休列表（进程生命周期内保活），保证 get() 返回的
// 裸指针在调用方一次 log 表达式内有效——正在进行的 backend->log() 不会撞上
// 悬空。退休列表只随注册表变更增长（注册一次的常规用法恒为空成本）。
std::unordered_map<std::string, std::shared_ptr<Logger>>& registry_map()
{
    static std::unordered_map<std::string, std::shared_ptr<Logger>> map;
    return map;
}

std::vector<std::shared_ptr<Logger>>& retired_loggers()
{
    static std::vector<std::shared_ptr<Logger>> retired;
    return retired;
}

std::shared_mutex& registry_mutex()
{
    static std::shared_mutex mtx;
    return mtx;
}

}   // namespace

void LoggerRegistry::register_logger(std::string_view target, std::shared_ptr<Logger> logger)
{
    std::string                         key(target);
    std::unique_lock<std::shared_mutex> lock(registry_mutex());
    if (logger == nullptr) {
        auto& map = registry_map();
        auto  it  = map.find(key);
        if (it != map.end()) {
            retired_loggers().push_back(std::move(it->second));
            map.erase(it);
        }
        return;
    }
    auto& map = registry_map();
    auto  it  = map.find(key);
    if (it != map.end())
        retired_loggers().push_back(std::move(it->second));
    map[std::move(key)] = std::move(logger);
}

Logger* LoggerRegistry::get(std::string_view target) noexcept
{
    std::shared_lock<std::shared_mutex> lock(registry_mutex());
    auto&                               map = registry_map();
    auto                                it  = map.find(std::string(target));
    if (it == map.end())
        return nullptr;
    return it->second.get();
}

bool LoggerRegistry::unregister_logger(std::string_view target) noexcept
{
    std::unique_lock<std::shared_mutex> lock(registry_mutex());
    auto&                               map = registry_map();
    auto                                it  = map.find(std::string(target));
    if (it == map.end())
        return false;
    retired_loggers().push_back(std::move(it->second));
    map.erase(it);
    return true;
}

void LoggerRegistry::clear() noexcept
{
    std::unique_lock<std::shared_mutex> lock(registry_mutex());
    auto&                               retired = retired_loggers();
    for (auto& entry : registry_map())
        retired.push_back(std::move(entry.second));
    registry_map().clear();
}

}   // namespace ca::log
