#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <thread>
#include <vector>

#include "libca/thread/once.hpp"

namespace ca::sync::test {
namespace {

TEST(OnceCellTest, UninitializedReportsEmpty)
{
    OnceCell<int> cell;

    EXPECT_FALSE(cell.is_initialized());
    EXPECT_EQ(cell.get(), nullptr);
}

TEST(OnceCellTest, GetOrInitRunsFactoryOnce)
{
    OnceCell<int> cell;
    int           factory_calls = 0;

    int& first  = cell.get_or_init([&]() {
        ++factory_calls;
        return 42;
    });
    int& second = cell.get_or_init([&]() {
        ++factory_calls;
        return 99;
    });

    EXPECT_EQ(factory_calls, 1);
    EXPECT_TRUE(cell.is_initialized());
    EXPECT_EQ(first, 42);
    EXPECT_EQ(second, 42);
    EXPECT_EQ(&first, &second);
}

TEST(OnceCellTest, TakeResetsToUninitialized)
{
    OnceCell<int> cell;
    cell.get_or_init([] { return 7; });

    auto taken = cell.take();
    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(*taken, 7);
    EXPECT_FALSE(cell.is_initialized());
    EXPECT_EQ(cell.take(), std::nullopt);
}

TEST(OnceCellTest, HoldsMovableOnlyType)
{
    OnceCell<std::unique_ptr<int>> cell;

    auto& value = cell.get_or_init([] { return std::make_unique<int>(5); });
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 5);
    EXPECT_TRUE(cell.is_initialized());
}

TEST(OnceLockTest, ConcurrentGetOrInitRunsFactoryOnce)
{
    OnceLock<int>    lock;
    std::atomic<int> factory_calls{0};
    constexpr int    thread_count = 8;

    auto worker = [&]() {
        int& value = lock.get_or_init([&]() {
            ++factory_calls;
            // 模拟耗时初始化，放大竞态窗口。
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return 123;
        });
        EXPECT_EQ(value, 123);
    };

    std::vector<std::future<void>> futures;
    futures.reserve(thread_count);
    for (int i = 0; i < thread_count; ++i)
        futures.push_back(std::async(std::launch::async, worker));
    for (auto& future : futures)
        future.get();

    EXPECT_EQ(factory_calls.load(), 1);
    EXPECT_TRUE(lock.is_initialized());
    EXPECT_EQ(*lock.get(), 123);
}

TEST(OnceLockTest, GetBeforeInitReturnsNull)
{
    OnceLock<int> lock;

    EXPECT_FALSE(lock.is_initialized());
    EXPECT_EQ(lock.get(), nullptr);
}

TEST(OnceLockTest, HoldsGlobalSingleton)
{
    // 模拟 issue 描述的全局单例用法：规避 static initialization order fiasco。
    static OnceLock<std::string> g_logger;

    auto& logger = g_logger.get_or_init([] { return std::string("app"); });
    EXPECT_EQ(logger, "app");

    auto& again = g_logger.get_or_init([] { return std::string("other"); });
    EXPECT_EQ(again, "app");
    EXPECT_EQ(&logger, &again);
}

}   // namespace
}   // namespace ca::sync::test
