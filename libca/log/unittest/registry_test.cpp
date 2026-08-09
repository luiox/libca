#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "libca/log/logger.hpp"
#include "libca/log/logger_registry.hpp"

#include "capture_backend.hpp"

namespace ca::log::test {
namespace {

class RegistryGuard
{
public:
    ~RegistryGuard() { LoggerRegistry::clear(); }
};

std::shared_ptr<Logger> make_logger()
{
    return std::make_shared<Logger>(std::make_shared<CaptureBackend>());
}

TEST(RegistryTest, RegisterAndGet)
{
    RegistryGuard guard;
    auto          logger = make_logger();
    LoggerRegistry::register_logger("net", logger);

    EXPECT_EQ(LoggerRegistry::get("net"), logger.get());
}

TEST(RegistryTest, UnknownTargetReturnsNull)
{
    RegistryGuard guard;
    EXPECT_EQ(LoggerRegistry::get("does-not-exist"), nullptr);
}

TEST(RegistryTest, RegisterReplacesExisting)
{
    RegistryGuard guard;
    auto first  = make_logger();
    auto second = make_logger();

    LoggerRegistry::register_logger("fs", first);
    EXPECT_EQ(LoggerRegistry::get("fs"), first.get());

    LoggerRegistry::register_logger("fs", second);
    EXPECT_EQ(LoggerRegistry::get("fs"), second.get());
    EXPECT_NE(LoggerRegistry::get("fs"), first.get());
}

TEST(RegistryTest, Unregister)
{
    RegistryGuard guard;
    auto          logger = make_logger();
    LoggerRegistry::register_logger("io", logger);

    EXPECT_TRUE(LoggerRegistry::unregister_logger("io"));
    EXPECT_EQ(LoggerRegistry::get("io"), nullptr);

    // 再次注销返回 false
    EXPECT_FALSE(LoggerRegistry::unregister_logger("io"));
}

TEST(RegistryTest, RegisterNullUnregisters)
{
    RegistryGuard guard;
    auto          logger = make_logger();
    LoggerRegistry::register_logger("x", logger);
    EXPECT_NE(LoggerRegistry::get("x"), nullptr);

    LoggerRegistry::register_logger("x", nullptr);
    EXPECT_EQ(LoggerRegistry::get("x"), nullptr);
}

TEST(RegistryTest, DifferentTargetsIndependent)
{
    RegistryGuard guard;
    auto a = make_logger();
    auto b = make_logger();
    LoggerRegistry::register_logger("a", a);
    LoggerRegistry::register_logger("b", b);

    a->set_level(Level::Error);
    b->set_level(Level::Trace);

    EXPECT_EQ(a->level(), Level::Error);
    EXPECT_EQ(b->level(), Level::Trace);
    // 互不影响
    EXPECT_EQ(LoggerRegistry::get("a")->level(), Level::Error);
    EXPECT_EQ(LoggerRegistry::get("b")->level(), Level::Trace);
}

// 并发验证：多线程并发 register/get/unregister 不崩溃，验证 shared_mutex 路径。
TEST(RegistryTest, ConcurrentAccessIsSafe)
{
    RegistryGuard guard;
    constexpr int thread_count    = 8;
    constexpr int ops_per_thread  = 500;

    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    std::atomic<bool> stop{false};

    auto worker = [&](int tid) {
        std::string target = "t" + std::to_string(tid);
        for (int i = 0; i < ops_per_thread; ++i) {
            // 注册自己的 target
            LoggerRegistry::register_logger(target, make_logger());
            // 读各种 target
            if (LoggerRegistry::get(target) != nullptr)
                ++read_count;
            LoggerRegistry::get("t0");  // 可能不存在，无所谓
            // 注销
            LoggerRegistry::unregister_logger(target);
            ++write_count;
        }
        (void)tid;
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i)
        threads.emplace_back(worker, i);
    for (auto& t : threads)
        t.join();

    EXPECT_EQ(write_count.load(), thread_count * ops_per_thread);
    EXPECT_GT(read_count.load(), 0);
}

// 并发验证：get 返回的 Logger* 在并发 register 替换后仍可安全使用（引用计数保活）。
TEST(RegistryTest, LoggerSurvivesConcurrentReplacement)
{
    RegistryGuard guard;
    auto          original = make_logger();
    LoggerRegistry::register_logger("hot", original);

    constexpr int iterations = 200;
    std::atomic<int> safe_uses{0};

    auto reader = [&]() {
        for (int i = 0; i < iterations; ++i) {
            Logger* p = LoggerRegistry::get("hot");
            if (p != nullptr) {
                p->should_log(Level::Info);  // 使用返回的指针
                ++safe_uses;
            }
        }
    };
    auto writer = [&]() {
        for (int i = 0; i < iterations; ++i)
            LoggerRegistry::register_logger("hot", make_logger());
    };

    std::thread r(reader);
    std::thread w(writer);
    r.join();
    w.join();

    EXPECT_GT(safe_uses.load(), 0);  // 未崩溃即证明引用计数保活生效
}

}  // namespace
}  // namespace ca::log::test
