#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "libca/thread/bounded_queue.hpp"

namespace ca::thread::test {
namespace {

using namespace std::chrono_literals;

TEST(BoundedQueueTest, RejectsZeroCapacity)
{
    auto queue = BoundedQueue<int>::create(0);

    ASSERT_TRUE(queue.is_err());
    EXPECT_EQ(queue.unwrap_err().code(), ca::core::StatusCode::INVALID_ARGUMENT);
}

TEST(BoundedQueueTest, PreservesFifoAndDrainsAfterClose)
{
    auto created = BoundedQueue<int>::create(3);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto queue = std::move(created).unwrap();

    EXPECT_TRUE(queue.push(1).is_ok());
    EXPECT_TRUE(queue.push(2).is_ok());
    EXPECT_TRUE(queue.close());
    EXPECT_FALSE(queue.close());

    auto first  = queue.pop();
    auto second = queue.pop();
    ASSERT_TRUE(first.is_ok());
    ASSERT_TRUE(second.is_ok());
    EXPECT_EQ(first.unwrap(), 1);
    EXPECT_EQ(second.unwrap(), 2);
    auto finished = queue.pop();
    ASSERT_TRUE(finished.is_err());
    EXPECT_EQ(finished.unwrap_err().code(), ca::core::StatusCode::CANCELLED);
}

TEST(BoundedQueueTest, TryAndTimedOperationsReportCapacityAndTimeout)
{
    auto created = BoundedQueue<int>::create(1);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto queue = std::move(created).unwrap();

    auto first = queue.try_push(1);
    ASSERT_TRUE(first.is_ok());
    EXPECT_TRUE(first.unwrap());
    auto full = queue.try_push(2);
    ASSERT_TRUE(full.is_ok());
    EXPECT_FALSE(full.unwrap());
    auto timed_push = queue.push_for(3, 1ms);
    ASSERT_TRUE(timed_push.is_ok());
    EXPECT_FALSE(timed_push.unwrap());

    ASSERT_TRUE(queue.pop().is_ok());
    auto timed_pop = queue.pop_for(1ms);
    ASSERT_TRUE(timed_pop.is_ok());
    EXPECT_FALSE(timed_pop.unwrap().has_value());
}

TEST(BoundedQueueTest, BlockingPushResumesAfterConsumerMakesSpace)
{
    auto created = BoundedQueue<int>::create(1);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto queue = std::move(created).unwrap();
    ASSERT_TRUE(queue.push(1).is_ok());

    auto producer = std::async(std::launch::async, [&]() { return queue.push(2); });
    EXPECT_EQ(producer.wait_for(10ms), std::future_status::timeout);
    EXPECT_EQ(queue.pop().unwrap(), 1);
    EXPECT_EQ(producer.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(producer.get().is_ok());
    EXPECT_EQ(queue.pop().unwrap(), 2);
}

TEST(BoundedQueueTest, CloseWakesBlockedProducerAndConsumer)
{
    auto producer_created = BoundedQueue<int>::create(1);
    ASSERT_TRUE(producer_created.is_ok());
    auto producer_queue = std::move(producer_created).unwrap();
    ASSERT_TRUE(producer_queue.push(1).is_ok());
    auto producer = std::async(std::launch::async, [&]() { return producer_queue.push(2); });

    auto consumer_created = BoundedQueue<int>::create(1);
    ASSERT_TRUE(consumer_created.is_ok());
    auto consumer_queue = std::move(consumer_created).unwrap();
    auto consumer       = std::async(std::launch::async, [&]() { return consumer_queue.pop(); });

    EXPECT_EQ(producer.wait_for(10ms), std::future_status::timeout);
    EXPECT_EQ(consumer.wait_for(10ms), std::future_status::timeout);
    producer_queue.close();
    consumer_queue.close();

    auto producer_status = producer.get();
    auto consumer_status = consumer.get();
    EXPECT_EQ(producer_status.code(), ca::core::StatusCode::FAILED_PRECONDITION);
    ASSERT_TRUE(consumer_status.is_err());
    EXPECT_EQ(consumer_status.unwrap_err().code(), ca::core::StatusCode::CANCELLED);
}

TEST(BoundedQueueTest, CloseAndTakeReturnsPendingMoveOnlyValues)
{
    auto created = BoundedQueue<std::unique_ptr<int>>::create(2);
    ASSERT_TRUE(created.is_ok());
    auto queue = std::move(created).unwrap();
    ASSERT_TRUE(queue.push(std::make_unique<int>(4)).is_ok());
    ASSERT_TRUE(queue.push(std::make_unique<int>(5)).is_ok());

    auto pending = queue.close_and_take();

    ASSERT_EQ(pending.size(), 2U);
    EXPECT_EQ(*pending[0], 4);
    EXPECT_EQ(*pending[1], 5);
    EXPECT_TRUE(queue.is_closed());
    EXPECT_EQ(queue.size(), 0U);
}

TEST(BoundedQueueTest, SupportsConcurrentProducersAndConsumers)
{
    auto created = BoundedQueue<int>::create(8);
    ASSERT_TRUE(created.is_ok());
    auto                     queue = std::move(created).unwrap();
    std::atomic<int>         count{0};
    std::atomic<int>         sum{0};
    std::vector<std::thread> consumers;
    for (int index = 0; index < 4; ++index) {
        consumers.emplace_back([&]() {
            for (;;) {
                auto value = queue.pop();
                if (value.is_err())
                    return;
                count.fetch_add(1, std::memory_order_relaxed);
                sum.fetch_add(value.unwrap(), std::memory_order_relaxed);
            }
        });
    }

    std::vector<std::thread> producers;
    for (int producer = 0; producer < 4; ++producer) {
        producers.emplace_back([&, producer]() {
            for (int value = 1; value <= 100; ++value)
                queue.push(producer * 100 + value);
        });
    }
    for (auto& producer : producers)
        producer.join();
    queue.close();
    for (auto& consumer : consumers)
        consumer.join();

    EXPECT_EQ(count.load(), 400);
    EXPECT_EQ(sum.load(), 80200);
}

}   // namespace
}   // namespace ca::thread::test
