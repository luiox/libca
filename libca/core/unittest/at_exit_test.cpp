#include <gtest/gtest.h>

#include "libca/core/at_exit.hpp"

#include <string>
#include <thread>
#include <vector>

namespace ca::core::test {

TEST(AtExitManagerTest, DestructorRunsCallbacksLifo) {
    std::string order;
    {
        AtExitManager manager;
        ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "a"; }));
        ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "b"; }));
        ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "c"; }));
        EXPECT_EQ(order, "");
    }
    EXPECT_EQ(order, "cba");
}

TEST(AtExitManagerTest, RunExecutesLifoAndClearsQueue) {
    std::string order;
    AtExitManager manager;
    ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "1"; }));
    ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "2"; }));

    AtExitManager::run();
    EXPECT_EQ(order, "21");

    // run() 之后队列已清空，再次 run 不重复执行。
    AtExitManager::run();
    EXPECT_EQ(order, "21");

    // run() 后 manager 仍可用，继续接收注册并在析构时执行。
    ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "3"; }));
    AtExitManager::run();
    EXPECT_EQ(order, "213");
}

TEST(AtExitManagerTest, RegisterWithoutManagerFails) {
    // 其他用例的 manager 都是语句块作用域，运行到这里时全局栈应为空。
    EXPECT_FALSE(AtExitManager::register_callback([]() {}));
}

TEST(AtExitManagerTest, RunOnlyConsumesCurrentManager) {
    std::string order;
    AtExitManager outer;
    ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "O"; }));
    {
        AtExitManager inner;
        ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "I"; }));
        // run() 只消费当前（栈顶 = inner）manager 的回调。
        AtExitManager::run();
        EXPECT_EQ(order, "I");
    }
    // inner 析构后队列已空、栈顶回落 outer；outer 的回调在其析构时执行。
    EXPECT_EQ(order, "I");
}

TEST(AtExitManagerTest, NestedManagerChainsBackToParent) {
    std::string order;
    AtExitManager outer;
    ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "A"; }));
    {
        AtExitManager inner;
        ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "B"; }));
        // inner 析构：先跑自己的回调（B），随后注册回落父级。
    }
    ASSERT_TRUE(AtExitManager::register_callback([&]() { order += "C"; }));
    AtExitManager::run();
    // outer 回调 LIFO：C 先于 A。
    EXPECT_EQ(order, "BCA");
}

TEST(AtExitManagerTest, EmptyCallbackIsSkippedSafely) {
    AtExitManager manager;
    EXPECT_TRUE(AtExitManager::register_callback(nullptr));
    EXPECT_NO_FATAL_FAILURE(AtExitManager::run());
}

TEST(AtExitManagerTest, CallbacksRegisteredDuringRunAreConsumed) {
    int count = 0;
    AtExitManager manager;
    // 第一个回调执行时再注册一个回调，验证分批消费不丢回调。
    ASSERT_TRUE(AtExitManager::register_callback([&]() {
        ++count;
        AtExitManager::register_callback([&]() { ++count; });
    }));
    AtExitManager::run();
    EXPECT_EQ(count, 2);
}

TEST(AtExitManagerTest, ThreadSafeRegistration) {
    constexpr int kThreads = 4;
    constexpr int kPerThread = 100;

    int counter = 0;
    {
        AtExitManager manager;
        {
            std::vector<std::thread> threads;
            for (int t = 0; t < kThreads; ++t) {
                threads.emplace_back([&]() {
                    for (int i = 0; i < kPerThread; ++i) {
                        EXPECT_TRUE(AtExitManager::register_callback([&]() { ++counter; }));
                    }
                });
            }
            for (auto& thread : threads) {
                thread.join();
            }
        }
        EXPECT_EQ(counter, 0);
    }
    EXPECT_EQ(counter, kThreads * kPerThread);
}

} // namespace ca::core::test
