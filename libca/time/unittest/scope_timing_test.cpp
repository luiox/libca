#include <gtest/gtest.h>

#include "libca/time/duration.hpp"
#include "libca/time/scope_timing.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace ca::time {

// 直接向注册表注入已知耗时，精确断言 count/total/max 聚合。
TEST(ScopeTimingTest, RegistryAggregatesCountTotalMaxExactly)
{
    ScopeTimingRegistry registry;
    registry.record("parse", Duration::from_milliseconds(5));
    registry.record("parse", Duration::from_milliseconds(9));
    registry.record("parse", Duration::from_milliseconds(2));

    const auto snapshot = registry.snapshot();
    ASSERT_EQ(snapshot.count("parse"), 1u);

    const ScopeTimingStats& stats = snapshot.at("parse");
    EXPECT_EQ(stats.count, 3u);
    EXPECT_EQ(stats.total.nanoseconds(), Duration::from_milliseconds(16).nanoseconds());
    EXPECT_EQ(stats.max.nanoseconds(), Duration::from_milliseconds(9).nanoseconds());
}

// 不同打点名分别聚合，snapshot 按名字升序排列。
TEST(ScopeTimingTest, SnapshotGroupsByNameAndSorted)
{
    ScopeTimingRegistry registry;
    registry.record("write", Duration::from_milliseconds(3));
    registry.record("parse", Duration::from_milliseconds(1));
    registry.record("parse", Duration::from_milliseconds(2));

    const auto snapshot = registry.snapshot();
    ASSERT_EQ(snapshot.size(), 2u);
    EXPECT_EQ(snapshot.begin()->first, "parse");
    EXPECT_EQ(std::next(snapshot.begin())->first, "write");
    EXPECT_EQ(snapshot.at("parse").count, 2u);
    EXPECT_EQ(snapshot.at("write").count, 1u);
}

// reset 清空全部统计。
TEST(ScopeTimingTest, ResetClearsAllStats)
{
    ScopeTimingRegistry registry;
    registry.record("a", Duration::from_milliseconds(1));
    registry.record("b", Duration::from_milliseconds(2));

    registry.reset();
    EXPECT_TRUE(registry.snapshot().empty());
    EXPECT_TRUE(registry.report().empty());
}

// RAII guard：作用域结束时把真实耗时累计进注入的注册表。
TEST(ScopeTimingTest, GuardRecordsElapsedIntoInjectedRegistry)
{
    ScopeTimingRegistry registry;
    {
        ScopeTiming timing("sleep_scope", registry);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto snapshot = registry.snapshot();
    ASSERT_EQ(snapshot.count("sleep_scope"), 1u);

    const ScopeTimingStats& stats = snapshot.at("sleep_scope");
    EXPECT_EQ(stats.count, 1u);
    EXPECT_GE(stats.total.nanoseconds(), Duration::from_milliseconds(10).nanoseconds());
    EXPECT_EQ(stats.max.nanoseconds(), stats.total.nanoseconds());
}

// 嵌套与连续作用域各自独立打点。
TEST(ScopeTimingTest, NestedAndSequentialScopesAggregate)
{
    ScopeTimingRegistry registry;
    {
        ScopeTiming outer("outer", registry);
        {
            ScopeTiming inner("inner", registry);
        }
        {
            ScopeTiming inner2("inner", registry);
        }
    }

    const auto snapshot = registry.snapshot();
    EXPECT_EQ(snapshot.at("outer").count, 1u);
    EXPECT_EQ(snapshot.at("inner").count, 2u);
}

// 未指定注册表时使用全局注册表，且与注入的注册表相互隔离。
TEST(ScopeTimingTest, DefaultGlobalRegistryAndInjectionIsolation)
{
    {
        ScopeTiming timing("scope_timing_global_probe");
    }
    EXPECT_EQ(ScopeTiming::global_registry().snapshot().count("scope_timing_global_probe"), 1u);

    ScopeTiming::global_registry().reset();

    ScopeTimingRegistry registry;
    {
        ScopeTiming timing("isolated", registry);
    }
    EXPECT_EQ(registry.snapshot().count("isolated"), 1u);
    EXPECT_EQ(ScopeTiming::global_registry().snapshot().count("isolated"), 0u);
}

// report 按行输出名字与 count/total/max 关键字段。
TEST(ScopeTimingTest, ReportContainsNamesAndFields)
{
    ScopeTimingRegistry registry;
    registry.record("parse", Duration::from_milliseconds(2));

    const std::string text = registry.report();
    EXPECT_NE(text.find("parse"), std::string::npos);
    EXPECT_NE(text.find("count=1"), std::string::npos);
    EXPECT_NE(text.find("total="), std::string::npos);
    EXPECT_NE(text.find("max="), std::string::npos);
}

}  // namespace ca::time
