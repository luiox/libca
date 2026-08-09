#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "libca/thread/channel.hpp"

namespace ca::thread::test {
namespace {

using namespace std::chrono_literals;

// 验收标准：单生产者单消费者基本收发。
TEST(ChannelTest, SingleProducerSingleConsumer)
{
    auto [tx, rx] = channel<int>();

    EXPECT_TRUE(tx.send(1));
    EXPECT_TRUE(tx.send(2));
    EXPECT_TRUE(tx.send(3));

    EXPECT_EQ(rx.recv(), std::optional<int>(1));
    EXPECT_EQ(rx.recv(), std::optional<int>(2));
    EXPECT_EQ(rx.recv(), std::optional<int>(3));
}

// 验收标准：多生产者单消费者（克隆 Sender）。
TEST(ChannelTest, MultipleProducersOneConsumer)
{
    auto [tx, rx] = channel<int>();
    Sender<int> tx2 = tx;  // 克隆 Sender
    Sender<int> tx3 = tx;

    EXPECT_TRUE(tx.send(10));
    EXPECT_TRUE(tx2.send(20));
    EXPECT_TRUE(tx3.send(30));

    std::vector<int> received;
    received.push_back(*rx.recv());
    received.push_back(*rx.recv());
    received.push_back(*rx.recv());

    // 顺序不定（多生产者），但三个值都应收到。
    EXPECT_THAT(received, ::testing::UnorderedElementsAre(10, 20, 30));
}

TEST(ChannelTest, TryRecvReturnsNulloptWhenEmpty)
{
    auto [tx, rx] = channel<int>();

    EXPECT_EQ(rx.try_recv(), std::nullopt);

    tx.send(42);
    EXPECT_EQ(rx.try_recv(), std::optional<int>(42));
    EXPECT_EQ(rx.try_recv(), std::nullopt);
}

// 验收标准：阻塞 recv 在无数据时等待。
TEST(ChannelTest, RecvBlocksUntilDataArrives)
{
    auto [tx, rx] = channel<int>();

    auto producer = std::async(std::launch::async, [&]() {
        std::this_thread::sleep_for(20ms);
        tx.send(99);
    });

    auto value = rx.recv();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 99);
    producer.get();
}

TEST(ChannelTest, RecvTimeoutReturnsNullopt)
{
    auto [tx, rx] = channel<int>();

    auto value = rx.recv_timeout(10ms);
    EXPECT_FALSE(value.has_value());
}

TEST(ChannelTest, RecvTimeoutReturnsValue)
{
    auto [tx, rx] = channel<int>();

    auto producer = std::async(std::launch::async, [&]() {
        tx.send(7);
    });

    auto value = rx.recv_timeout(100ms);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 7);
    producer.get();
}

// 验收标准：Sender 全析构后 Receiver 收到空信号。
TEST(ChannelTest, AllSendersDroppedReturnsNullopt)
{
    auto [tx, rx] = channel<int>();
    {
        Sender<int> tx2 = tx;
        tx.send(1);
        tx2.send(2);
    }
    // tx2 销毁，但 tx 仍存活——先把它也销毁，模拟"全部生产者离开"。
    tx.close();

    EXPECT_EQ(rx.recv(), std::optional<int>(1));
    EXPECT_EQ(rx.recv(), std::optional<int>(2));
    // 排空后，无生产者（显式关闭），recv 返回 nullopt。
    EXPECT_EQ(rx.recv(), std::nullopt);
}

// 验收标准(5) 的真析构路径：不调用 close()，让唯一一个 Sender 自然析构
// （sender_count 归 0 → release() 中的 producers_alive() 为假 → notify 消费者）。
// 覆盖与上面 close() 路径不同的分支。
TEST(ChannelTest, LastSenderDtorWithoutCloseSignalsReceiver)
{
    Receiver<int> rx = Receiver<int>{};  // 先占位，下面作用域内绑定
    {
        auto [tx, rx_local] = channel<int>();
        tx.send(42);
        rx = std::move(rx_local);
        // tx 在此作用域结束时自然析构：sender_count 1→0，无 close()。
    }

    // 排空已入队元素。
    EXPECT_EQ(rx.recv(), std::optional<int>(42));
    // 队列空 + 无生产者（未 close，纯靠析构），recv 应回溯返回 nullopt。
    EXPECT_EQ(rx.recv(), std::nullopt);
}

TEST(ChannelTest, ExplicitCloseMakesRecvDrainAndReturnNullopt)
{
    auto [tx, rx] = channel<int>();
    tx.send(10);
    tx.close();  // 显式关闭，Sender 仍在

    EXPECT_EQ(rx.recv(), std::optional<int>(10));
    EXPECT_EQ(rx.recv(), std::nullopt);  // 关闭后排空 -> nullopt
}

// 验收标准：有界通道在满时阻塞 send。
TEST(ChannelTest, BoundedChannelBlocksSendWhenFull)
{
    auto [tx, rx] = channel<int>(2);

    EXPECT_TRUE(tx.send(1));
    EXPECT_TRUE(tx.send(2));
    // 队列已满（容量 2）。异步消费者取走一个后，send 才能成功。
    auto consumer = std::async(std::launch::async, [&]() {
        std::this_thread::sleep_for(20ms);
        rx.recv();
    });

    EXPECT_TRUE(tx.send(3));  // 阻塞直到消费者取走 1
    consumer.get();
}

TEST(ChannelTest, BoundedChannelSendFailsWhenReceiverDropped)
{
    auto [tx, rx] = channel<int>(1);
    tx.send(1);  // 填满有界队列

    // 销毁 Receiver：有界 send 不应死锁，应立即返回 false。
    Receiver<int> rx_moved = std::move(rx);  // rx 置空
    // rx_moved 析构 -> receiver_alive=false，唤醒阻塞的生产者。
    rx_moved = Receiver<int>{};              // 强制销毁接收端状态

    EXPECT_FALSE(tx.send(2));
}

TEST(ChannelTest, UnboundedChannelAcceptsMany)
{
    auto [tx, rx] = channel<int>(0);  // 无界

    for (int i = 0; i < 1000; ++i)
        ASSERT_TRUE(tx.send(i));

    for (int i = 0; i < 1000; ++i)
        EXPECT_EQ(*rx.recv(), i);
}

TEST(ChannelTest, EmptySenderSendReturnsFalse)
{
    Sender<int> tx;  // 空发送端
    EXPECT_FALSE(tx.send(1));
}

TEST(ChannelTest, EmptyReceiverRecvReturnsNullopt)
{
    Receiver<int> rx;  // 空接收端
    EXPECT_EQ(rx.recv(), std::nullopt);
}

// 并发压测：多生产者并发 send，单消费者收齐全。
TEST(ChannelTest, ConcurrentProducersDeliverAll)
{
    constexpr int producer_count = 4;
    constexpr int per_producer   = 100;

    auto [tx, rx] = channel<int>(0);

    std::vector<std::future<void>> producers;
    for (int p = 0; p < producer_count; ++p) {
        producers.push_back(std::async(std::launch::async, [&, p]() {
            for (int i = 0; i < per_producer; ++i)
                tx.send(p * per_producer + i);
        }));
    }

    std::vector<int> received;
    received.reserve(producer_count * per_producer);
    for (int i = 0; i < producer_count * per_producer; ++i)
        received.push_back(*rx.recv());

    for (auto& f : producers)
        f.get();

    std::sort(received.begin(), received.end());
    std::vector<int> expected;
    expected.reserve(producer_count * per_producer);
    for (int i = 0; i < producer_count * per_producer; ++i)
        expected.push_back(i);
    EXPECT_EQ(received, expected);
}

}  // namespace
}  // namespace ca::thread::test
