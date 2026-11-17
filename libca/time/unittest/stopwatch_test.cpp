#include <gtest/gtest.h>

#include "libca/time/duration.hpp"
#include "libca/time/stopwatch.hpp"
#include "steady_clock_test_support.hpp"

#include <chrono>

namespace ca::time {

TEST(StopwatchTest, StartsOnConstructionAndElapsedGrows)
{
    Stopwatch watch;
    EXPECT_TRUE(watch.is_running());

    // elapsed 非负且随真实时间单调不减
    const Duration first = watch.elapsed();
    test::wait_for_elapsed(std::chrono::milliseconds(10));
    const Duration second = watch.elapsed();

    EXPECT_GE(first.nanoseconds(), 0);
    EXPECT_GE(second.nanoseconds(), first.nanoseconds());
    EXPECT_GE(second.nanoseconds(), Duration::from_milliseconds(10).nanoseconds());
}

TEST(StopwatchTest, RestartResumesFromZero)
{
    Stopwatch watch;
    test::wait_for_elapsed(std::chrono::milliseconds(10));
    const Duration before = watch.elapsed();
    ASSERT_GE(before.nanoseconds(), Duration::from_milliseconds(10).nanoseconds());

    const auto restart_started = std::chrono::steady_clock::now();
    watch.restart();
    const Duration after = watch.elapsed();
    const auto restart_finished = std::chrono::steady_clock::now();

    // 新耗时只包含 restart 之后的时间，即使线程在调用之间被抢占。
    EXPECT_TRUE(watch.is_running());
    EXPECT_GE(after.nanoseconds(), 0);
    EXPECT_LE(after, Duration::from_chrono(restart_finished - restart_started));

    // restart 后继续正常计时
    test::wait_for_elapsed(std::chrono::milliseconds(5));
    EXPECT_GE(watch.elapsed().nanoseconds(), Duration::from_milliseconds(5).nanoseconds());
}

TEST(StopwatchTest, ResetStopsAndFreezesAtZero)
{
    Stopwatch watch;
    test::wait_for_elapsed(std::chrono::milliseconds(5));
    watch.reset();

    EXPECT_FALSE(watch.is_running());
    EXPECT_TRUE(watch.elapsed().is_zero());

    // 停表状态下耗时冻结为 0
    test::wait_for_elapsed(std::chrono::milliseconds(5));
    EXPECT_TRUE(watch.elapsed().is_zero());

    // restart 后恢复计时
    watch.restart();
    EXPECT_TRUE(watch.is_running());
    test::wait_for_elapsed(std::chrono::milliseconds(5));
    EXPECT_GE(watch.elapsed().nanoseconds(), Duration::from_milliseconds(5).nanoseconds());
}

TEST(StopwatchTest, ElapsedReturnsDurationValueSemantics)
{
    Stopwatch watch;
    const Duration a = watch.elapsed();
    const Duration b = watch.elapsed();

    // 纯值类型：两次 elapsed 返回可比较、可运算的 Duration
    EXPECT_NO_FATAL_FAILURE(static_cast<void>(a + b));
    EXPECT_LE(a, b);
}

}  // namespace ca::time
