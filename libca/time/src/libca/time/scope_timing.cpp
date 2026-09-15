#include "libca/time/scope_timing.hpp"

#include "libca/time/time_util.hpp"

#include <algorithm>
#include <mutex>
#include <sstream>

namespace ca::time {

void ScopeTimingRegistry::record(std::string_view name, Duration elapsed)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& entry = stats_[std::string(name)];
    entry.count += 1;
    entry.total += elapsed;
    entry.max = std::max(entry.max, elapsed);
}

std::map<std::string, ScopeTimingStats> ScopeTimingRegistry::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::string ScopeTimingRegistry::report() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream out;
    for (const auto& [name, entry] : stats_) {
        // 统一用毫秒带三位小数展示，保留纳秒级精度信息且便于跨数量级阅读。
        const double total_ms = static_cast<double>(entry.total.nanoseconds()) / 1000000.0;
        const double max_ms = static_cast<double>(entry.max.nanoseconds()) / 1000000.0;
        out << name << ": count=" << entry.count << " total=" << total_ms << "ms max=" << max_ms << "ms\n";
    }
    return out.str();
}

void ScopeTimingRegistry::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.clear();
}

ScopeTimingRegistry& ScopeTiming::global_registry()
{
    // C++11 起 static 局部变量初始化线程安全，作为进程级默认注册表单例。
    static ScopeTimingRegistry registry;
    return registry;
}

ScopeTiming::ScopeTiming(std::string name, ScopeTimingRegistry& registry)
    : name_(std::move(name)), registry_(&registry), start_nanos_(TimeUtil::nano_time())
{
}

ScopeTiming::~ScopeTiming()
{
    registry_->record(name_, Duration::from_nanoseconds(TimeUtil::nano_time() - start_nanos_));
}

}  // namespace ca::time
