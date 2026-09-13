#pragma once

#include "libca/core/datatype.hpp"
#include "libca/time/duration.hpp"

#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace ca::time {

/// @brief 单个名字的耗时聚合统计：调用次数、总耗时、最大单次耗时。
struct ScopeTimingStats {
    /// 累计打点次数。
    ca::usize count{0};
    /// 所有打点的耗时总和。
    Duration total{};
    /// 单次耗时的最大值。
    Duration max{};
};

/// @brief 按名字聚合耗时的线程安全注册表。
///
/// `ScopeTimingRegistry` 保存名字到 `ScopeTimingStats` 的映射，`record()` 可被
/// 多线程并发调用（内部以 std::mutex 保护）。`snapshot()`/`report()`/`reset()`
/// 供测试与运维读取。
class ScopeTimingRegistry {
public:
    ScopeTimingRegistry() = default;

    // 注册表含互斥量，禁止拷贝与移动。
    ScopeTimingRegistry(const ScopeTimingRegistry&) = delete;
    ScopeTimingRegistry& operator=(const ScopeTimingRegistry&) = delete;

    /// @brief 记录一次指定名字的耗时（线程安全）。
    /// @param name 打点名；内部以字符串拷贝保存。
    /// @param elapsed 本次耗时。
    void record(std::string_view name, Duration elapsed);

    /// @brief 返回按名字升序排列的统计快照。
    std::map<std::string, ScopeTimingStats> snapshot() const;

    /// @brief 生成可读报告文本；每个名字一行，格式为
    ///        `name: count=N total=X.XXXms max=Y.YYYms`，按名字升序排列。
    std::string report() const;

    /// @brief 清空所有统计。
    void reset();

private:
    // 保护 stats_；声明为 mutable 以支持 const 查询接口。
    mutable std::mutex mutex_;
    std::map<std::string, ScopeTimingStats> stats_;
};

/// @brief RAII 耗时打点 guard：构造时登记名字并开始计时，析构把耗时累计进注册表。
///
/// 典型用法（默认使用进程级全局注册表）：
///
/// @code
/// void parse() {
///     ScopeTiming timing("parse");
///     // ... 被统计的代码 ...
/// }
/// @endcode
///
/// 测试或需要分组统计时，可注入自定义注册表。guard 与词法作用域绑定，
/// 不可拷贝、不可移动。
class ScopeTiming {
public:
    /// @brief 开始一段耗时打点。
    /// @param name 打点名，析构时按该名字累计进注册表。
    /// @param registry 目标注册表；缺省使用进程级全局注册表。
    explicit ScopeTiming(std::string name, ScopeTimingRegistry& registry = global_registry());

    /// @brief 析构时把构造到析构的耗时累计进注册表。
    ~ScopeTiming();

    ScopeTiming(const ScopeTiming&) = delete;
    ScopeTiming& operator=(const ScopeTiming&) = delete;

    /// @brief 返回进程级默认全局注册表。
    static ScopeTimingRegistry& global_registry();

private:
    std::string name_;
    ScopeTimingRegistry* registry_;
    ca::i64 start_nanos_;
};

}  // namespace ca::time
