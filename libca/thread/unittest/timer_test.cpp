#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <thread>

#include "libca/thread/timer.hpp"

namespace ca::thread::test {
namespace {

using namespace std::chrono_literals;

// 简单等待 future 到位，超时返回 false，避免测试因调度抖动而挂死。
template<typename Future>
bool wait_ready(const Future& future, std::chrono::milliseconds timeout)
{
    return future.wait_for(timeout) == std::future_status::ready;
}

// promise 是 move-only 的，跨入可复制的定时器回调需要 shared_ptr 包裹。
using VoidPromise = std::shared_ptr<std::promise<void>>;

VoidPromise make_promise()
{
    return std::make_shared<std::promise<void>>();
}

TEST(TimerManagerTest, FiresOnceAfterDelay)
{
    TimerManager      manager;
    std::atomic<int>  count{0};
    VoidPromise       fired = make_promise();
    auto              signal = fired->get_future();

    auto handle = manager.schedule_once(30ms, [&count, fired] {
        if (count.fetch_add(1) == 0)
            fired->set_value();
    });
    ASSERT_TRUE(static_cast<bool>(handle));

    ASSERT_TRUE(wait_ready(signal, 2s));
    std::this_thread::sleep_for(60ms);   // 负向观察窗口：一次性任务不应重复触发
    EXPECT_EQ(count.load(), 1);
}

TEST(TimerManagerTest, CancelPreventsCallback)
{
    TimerManager      manager;
    std::atomic<int>  count{0};
    VoidPromise       sentinel = make_promise();
    auto              signal   = sentinel->get_future();

    auto target = manager.schedule_once(50ms, [&count] { ++count; });
    ASSERT_TRUE(static_cast<bool>(target));
    target.cancel();

    // 哨兵定时器推进时间越过目标到期点，为“不应触发”提供确定观察时刻。
    auto guard = manager.schedule_once(150ms, [sentinel] { sentinel->set_value(); });
    ASSERT_TRUE(static_cast<bool>(guard));
    ASSERT_TRUE(wait_ready(signal, 2s));

    EXPECT_EQ(count.load(), 0);
    EXPECT_FALSE(manager.next_expiry().has_value());   // 已取消任务被及时移出队列
}

TEST(TimerManagerTest, RepeatingReschedulesUntilCallbackReturnsFalse)
{
    TimerManager     manager;
    std::atomic<int> count{0};

    auto handle = manager.schedule_repeating(20ms, [&count]() -> bool {
        return count.fetch_add(1) + 1 < 3;   // 第 3 次返回 false 停止
    });
    ASSERT_TRUE(static_cast<bool>(handle));

    auto deadline = std::chrono::steady_clock::now() + 2s;
    while (count.load() < 3 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(2ms);
    ASSERT_GE(count.load(), 3);

    std::this_thread::sleep_for(80ms);   // 返回 false 后不应继续触发
    EXPECT_EQ(count.load(), 3);
}

TEST(TimerManagerTest, CallbackMayCancelItself)
{
    TimerManager     manager;
    std::atomic<int> count{0};

    // armed_future 保证首次回调执行时主线程已完成句柄捕获，消除测试自身的初始化竞争。
    std::promise<void> armed;
    auto               armed_future = armed.get_future().share();
    std::optional<TimerHandle> slot;

    auto handle = manager.schedule_repeating(20ms, [&, armed_future]() -> bool {
        ++count;
        armed_future.wait();
        if (slot.has_value())
            slot->cancel();   // 回调内自取消，不应死锁
        return true;          // 句柄已取消，返回 true 也不应续期
    });
    ASSERT_TRUE(static_cast<bool>(handle));
    slot = handle;
    armed.set_value();

    VoidPromise sentinel = make_promise();
    auto        signal   = sentinel->get_future();
    auto        guard    = manager.schedule_once(150ms, [sentinel] { sentinel->set_value(); });
    ASSERT_TRUE(static_cast<bool>(guard));
    ASSERT_TRUE(wait_ready(signal, 2s));

    EXPECT_EQ(count.load(), 1);   // 自取消后不再触发
}

TEST(TimerManagerTest, CallbackMayScheduleNewTimer)
{
    TimerManager manager;
    VoidPromise  inner_fired = make_promise();
    auto         signal      = inner_fired->get_future();
    std::atomic<bool> inner_valid{false};

    auto outer = manager.schedule_once(20ms, [&inner_valid, inner_fired] {
        // 回调内安排新任务，不应死锁；新任务正常触发。
        auto inner = manager.schedule_once(10ms, [inner_fired] { inner_fired->set_value(); });
        inner_valid.store(static_cast<bool>(inner));
    });
    ASSERT_TRUE(static_cast<bool>(outer));

    ASSERT_TRUE(wait_ready(signal, 2s));
    EXPECT_TRUE(inner_valid.load());
}

TEST(TimerManagerTest, CancelDoesNotInterruptRunningCallback)
{
    TimerManager      manager;
    std::atomic<bool> finished{false};

    auto started = make_promise();
    auto release = make_promise();
    auto done    = make_promise();
    auto started_future = started->get_future();
    auto done_future    = done->get_future();

    auto handle = manager.schedule_repeating(
        30ms,
        [&, started, release, done]() -> bool {
            started->set_value();
            release->get_future().wait();   // 阻塞期间被 cancel，也不应被中断
            finished.store(true);
            done->set_value();
            return false;
        });
    ASSERT_TRUE(static_cast<bool>(handle));

    ASSERT_EQ(started_future.wait_for(2s), std::future_status::ready);
    handle.cancel();   // 回调执行中取消：不阻塞、不中断本次执行
    release->set_value();

    ASSERT_EQ(done_future.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(finished.load());   // 回调完整执行到结束
    EXPECT_FALSE(manager.next_expiry().has_value());
}

TEST(TimerManagerTest, DestructorStopsSchedulingAndJoins)
{
    std::atomic<int> count{0};
    {
        TimerManager manager;
        auto         handle = manager.schedule_repeating(10ms, [&count]() -> bool {
            ++count;
            return true;
        });
        ASSERT_TRUE(static_cast<bool>(handle));

        auto deadline = std::chrono::steady_clock::now() + 2s;
        while (count.load() < 2 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(2ms);
        ASSERT_GE(count.load(), 2);
    }   // 析构：取消未到期任务并 join 调度线程，不应挂死

    const int after_destroy = count.load();
    std::this_thread::sleep_for(60ms);
    EXPECT_EQ(count.load(), after_destroy);   // 析构后不再触发
}

TEST(TimerManagerTest, NextExpiryReflectsEarliestTimer)
{
    TimerManager manager;
    EXPECT_FALSE(manager.next_expiry().has_value());

    const auto start = std::chrono::steady_clock::now();
    auto       first = manager.schedule_once(200ms, [] {});
    ASSERT_TRUE(static_cast<bool>(first));
    auto second = manager.schedule_once(50ms, [] {});
    ASSERT_TRUE(static_cast<bool>(second));

    auto expiry = manager.next_expiry();
    ASSERT_TRUE(expiry.has_value());
    EXPECT_GE(*expiry, start + 50ms);    // 到期时间 = 安排时刻 + delay
    EXPECT_LE(*expiry, start + 200ms);   // 返回最近到期者，而非任一任务

    second.cancel();
    expiry = manager.next_expiry();
    ASSERT_TRUE(expiry.has_value());
    EXPECT_GE(*expiry, start + 200ms);   // 取消最近者后回退到次近者

    first.cancel();
    EXPECT_FALSE(manager.next_expiry().has_value());
}

TEST(TimerManagerTest, RejectsInvalidArguments)
{
    TimerManager manager;

    EXPECT_FALSE(static_cast<bool>(manager.schedule_once(50ms, nullptr)));
    EXPECT_FALSE(static_cast<bool>(manager.schedule_repeating(50ms, nullptr)));
    EXPECT_FALSE(static_cast<bool>(manager.schedule_repeating(0ms, [] { return true; })));
    EXPECT_FALSE(static_cast<bool>(manager.schedule_repeating(-10ms, [] { return true; })));

    // 负延迟按 0 处理：尽快触发。
    VoidPromise fired  = make_promise();
    auto        signal = fired->get_future();
    auto        handle = manager.schedule_once(-100ms, [fired] { fired->set_value(); });
    ASSERT_TRUE(static_cast<bool>(handle));
    EXPECT_TRUE(wait_ready(signal, 2s));
}

}  // namespace
}  // namespace ca::thread::test
