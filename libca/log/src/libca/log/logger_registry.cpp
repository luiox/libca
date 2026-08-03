#include "libca/log/logger_registry.hpp"

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace ca::log {

namespace {

// Meyers 单例：局部 static 保证首次访问初始化线程安全。
// 持有 shared_ptr<Logger> 而非裸指针，register 替换后旧 Logger 由引用计数保活，
// 正在进行的 backend->log() 调用不会撞上悬空。
std::unordered_map<std::string, std::shared_ptr<Logger>>& registry_map()
{
    static std::unordered_map<std::string, std::shared_ptr<Logger>> map;
    return map;
}

std::shared_mutex& registry_mutex()
{
    static std::shared_mutex mtx;
    return mtx;
}

}  // namespace

void LoggerRegistry::register_logger(std::string_view target, std::shared_ptr<Logger> logger)
{
    std::string key(target);
    std::unique_lock<std::shared_mutex> lock(registry_mutex());
    if (logger == nullptr) {
        registry_map().erase(key);
        return;
    }
    registry_map()[std::move(key)] = std::move(logger);
}

Logger* LoggerRegistry::get(std::string_view target) noexcept
{
    std::shared_lock<std::shared_mutex> lock(registry_mutex());
    auto& map = registry_map();
    auto  it  = map.find(std::string(target));
    if (it == map.end())
        return nullptr;
    return it->second.get();
}

bool LoggerRegistry::unregister_logger(std::string_view target) noexcept
{
    std::unique_lock<std::shared_mutex> lock(registry_mutex());
    auto& map = registry_map();
    auto  it  = map.find(std::string(target));
    if (it == map.end())
        return false;
    map.erase(it);
    return true;
}

void LoggerRegistry::clear() noexcept
{
    std::unique_lock<std::shared_mutex> lock(registry_mutex());
    registry_map().clear();
}

}  // namespace ca::log
