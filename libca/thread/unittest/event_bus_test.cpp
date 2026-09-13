#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

#include "libca/thread/event_bus.hpp"

namespace ca::thread::test {
namespace {

TEST(EventBusTest, DeliversToAllSubscribers)
{
    EventBus bus;
    int first = 0;
    int second = 0;
    auto h1 = bus.subscribe("login", [&] { ++first; });
    auto h2 = bus.subscribe("login", [&] { ++second; });
    ASSERT_TRUE(static_cast<bool>(h1));
    ASSERT_TRUE(static_cast<bool>(h2));

    EXPECT_EQ(bus.emit("login"), 2U);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 1);
    EXPECT_EQ(bus.emit("nobody-listens"), 0U);   // 无监听者时安全返回 0
}

TEST(EventBusTest, UnsubscribeStopsDelivery)
{
    EventBus bus;
    int count = 0;
    auto handle = bus.subscribe("tick", [&] { ++count; });
    ASSERT_TRUE(static_cast<bool>(handle));

    bus.unsubscribe(handle);
    handle.unsubscribe();                     // 幂等
    bus.unsubscribe(SubscriptionHandle{});    // 无效句柄安全空操作

    EXPECT_EQ(bus.emit("tick"), 0U);
    EXPECT_EQ(count, 0);
}

TEST(EventBusTest, IsolatesListenerExceptions)
{
    EventBus bus;
    int survived = 0;
    bus.subscribe("boom", [] { throw std::runtime_error("listener failed"); });
    bus.subscribe("boom", [&] { ++survived; });

    EXPECT_NO_THROW(bus.emit("boom"));        // 异常不逃出 emit
    EXPECT_EQ(bus.emit("boom"), 2U);          // 抛异常的监听器也计入调用数
    EXPECT_EQ(survived, 2);                   // 其余监听器照常调用
}

TEST(EventBusTest, CallbackMayEmitOtherEvent)
{
    EventBus bus;
    int chained = 0;
    bus.subscribe("outer", [&] { bus.emit("inner"); });   // 回调内 emit 其它事件，不死锁
    bus.subscribe("inner", [&] { ++chained; });

    EXPECT_EQ(bus.emit("outer"), 1U);
    EXPECT_EQ(chained, 1);
}

TEST(EventBusTest, UnsubscribeDuringEmitUsesSnapshot)
{
    EventBus bus;
    int calls = 0;
    SubscriptionHandle victim;
    bus.subscribe("event", [&] { victim.unsubscribe(); });   // 回调内注销另一监听器
    victim = bus.subscribe("event", [&] { ++calls; });

    EXPECT_EQ(bus.emit("event"), 2U);   // 快照语义：本轮仍调用已注销的监听器
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(bus.emit("event"), 1U);   // 之后的 emit 不再调用
    EXPECT_EQ(calls, 1);
}

TEST(EventBusTest, RejectsEmptyCallbackAndAllowsDuplicateSubscription)
{
    EventBus bus;
    EXPECT_FALSE(static_cast<bool>(bus.subscribe("e", nullptr)));

    int count = 0;
    auto first = bus.subscribe("e", [&] { ++count; });
    auto second = bus.subscribe("e", [&] { ++count; });
    EXPECT_TRUE(static_cast<bool>(first));
    EXPECT_TRUE(static_cast<bool>(second));
    EXPECT_EQ(bus.emit("e"), 2U);
    EXPECT_EQ(count, 2);

    first.unsubscribe();
    second.unsubscribe();
    EXPECT_EQ(bus.emit("e"), 0U);
}

TEST(EventBusTest, ConcurrentSubscribeEmitUnsubscribe)
{
    EventBus bus;
    std::atomic<int> total{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&bus, &total] {
            for (int i = 0; i < 100; ++i) {
                auto handle = bus.subscribe("stress", [&total] { ++total; });
                bus.emit("stress");
                handle.unsubscribe();
            }
        });
    }
    for (auto& worker : workers)
        worker.join();

    EXPECT_EQ(bus.emit("stress"), 0U);   // 全部注销后无人监听
}

}  // namespace
}  // namespace ca::thread::test
