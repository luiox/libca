#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "libca/thread/thread_pool.hpp"

namespace ca::thread::test {
namespace {

using namespace std::chrono_literals;

class ThrowingMoveCallable
{
public:
    ThrowingMoveCallable()                            = default;
    ThrowingMoveCallable(const ThrowingMoveCallable&) = delete;
    ThrowingMoveCallable(ThrowingMoveCallable&&)
    {
        throw std::runtime_error("callable move failed");
    }

    void operator()() const {}
};

ThreadPool create_pool(usize thread_count, usize queue_capacity)
{
    ThreadPoolOptions options;
    options.thread_count   = thread_count;
    options.queue_capacity = queue_capacity;
    auto created           = ThreadPool::create(options);
    if (created.is_err())
        throw std::runtime_error(created.unwrap_err().to_string());
    return std::move(created).unwrap();
}

TEST(ThreadPoolTest, RejectsZeroQueueCapacity)
{
    ThreadPoolOptions options;
    options.queue_capacity = 0;

    auto created = ThreadPool::create(options);

    ASSERT_TRUE(created.is_err());
    EXPECT_EQ(created.unwrap_err().code(), ca::core::StatusCode::INVALID_ARGUMENT);
}

TEST(ThreadPoolTest, ZeroThreadCountUsesAutomaticWorkerCount)
{
    auto pool = create_pool(0, 1);

    EXPECT_GE(pool.worker_count(), 1U);
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, PropagatesReturnValuesAndSupportsVoidTasks)
{
    auto pool = create_pool(2, 4);

    auto value_result = pool.submit([] { return 42; });
    auto void_result  = pool.submit([] {});
    ASSERT_TRUE(value_result.is_ok());
    ASSERT_TRUE(void_result.is_ok());
    auto value_future = std::move(value_result).unwrap();
    auto void_future  = std::move(void_result).unwrap();

    EXPECT_EQ(value_future.get(), 42);
    EXPECT_NO_THROW(void_future.get());
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, AcceptsMoveOnlyCallable)
{
    auto pool      = create_pool(1, 1);
    auto submitted = pool.submit([value = std::make_unique<int>(6)] { return *value; });
    ASSERT_TRUE(submitted.is_ok());
    auto future = std::move(submitted).unwrap();

    EXPECT_EQ(future.get(), 6);
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, ReportsCallableSetupExceptionAsInternalError)
{
    auto pool = create_pool(1, 1);

    auto submitted = pool.submit(ThrowingMoveCallable{});

    ASSERT_TRUE(submitted.is_err());
    EXPECT_EQ(submitted.unwrap_err().code(), ca::core::StatusCode::INTERNAL);
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, SuppliesSharedStopTokenToTasks)
{
    auto pool = create_pool(1, 1);

    auto submitted = pool.submit([expected = pool.stop_token()](StopToken actual) {
        return expected.stop_possible() && actual.stop_possible() &&
               expected.stop_requested() == actual.stop_requested();
    });
    ASSERT_TRUE(submitted.is_ok());
    auto future = std::move(submitted).unwrap();

    EXPECT_TRUE(future.get());
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, TaskExceptionStaysInFutureAndWorkerContinues)
{
    auto pool = create_pool(1, 2);

    auto failed = pool.submit([]() -> int { throw std::runtime_error("task failure"); });
    auto next   = pool.submit([] { return 7; });
    ASSERT_TRUE(failed.is_ok());
    ASSERT_TRUE(next.is_ok());
    auto failed_future = std::move(failed).unwrap();
    auto next_future   = std::move(next).unwrap();

    EXPECT_THROW(failed_future.get(), std::runtime_error);
    EXPECT_EQ(next_future.get(), 7);
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, BlockingSubmitAppliesBackpressure)
{
    auto               pool = create_pool(1, 1);
    std::promise<void> entered;
    std::promise<void> release;
    auto               release_signal = release.get_future().share();

    auto running = pool.submit([&] {
        entered.set_value();
        release_signal.wait();
    });
    ASSERT_TRUE(running.is_ok());
    ASSERT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    auto queued = pool.submit([] { return 2; });
    ASSERT_TRUE(queued.is_ok());

    auto blocked = std::async(std::launch::async, [&] { return pool.submit([] { return 3; }); });
    EXPECT_EQ(blocked.wait_for(20ms), std::future_status::timeout);
    release.set_value();
    ASSERT_EQ(blocked.wait_for(1s), std::future_status::ready);
    auto third = blocked.get();
    ASSERT_TRUE(third.is_ok());

    auto running_future = std::move(running).unwrap();
    auto queued_future  = std::move(queued).unwrap();
    auto third_future   = std::move(third).unwrap();
    EXPECT_NO_THROW(running_future.get());
    EXPECT_EQ(queued_future.get(), 2);
    EXPECT_EQ(third_future.get(), 3);
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, NonblockingAndTimedSubmitReportFullQueue)
{
    auto               pool = create_pool(1, 1);
    std::promise<void> entered;
    std::promise<void> release;
    auto               release_signal = release.get_future().share();

    auto running = pool.submit([&] {
        entered.set_value();
        release_signal.wait();
    });
    ASSERT_TRUE(running.is_ok());
    ASSERT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    auto queued = pool.submit([] {});
    ASSERT_TRUE(queued.is_ok());

    auto immediate = pool.try_submit([] {});
    ASSERT_TRUE(immediate.is_ok());
    EXPECT_FALSE(std::move(immediate).unwrap().has_value());
    auto timed = pool.submit_for([] {}, 1ms);
    ASSERT_TRUE(timed.is_ok());
    EXPECT_FALSE(std::move(timed).unwrap().has_value());

    release.set_value();
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, DrainExecutesAllAcceptedTasks)
{
    auto                           pool = create_pool(2, 16);
    std::atomic<int>               completed{0};
    std::vector<std::future<void>> futures;
    for (int index = 0; index < 16; ++index) {
        auto submitted = pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
        ASSERT_TRUE(submitted.is_ok());
        futures.push_back(std::move(submitted).unwrap());
    }

    EXPECT_TRUE(pool.shutdown(ShutdownMode::Drain).is_ok());
    EXPECT_TRUE(pool.join().is_ok());
    EXPECT_EQ(completed.load(std::memory_order_relaxed), 16);
    for (auto& future : futures)
        EXPECT_NO_THROW(future.get());
}

TEST(ThreadPoolTest, CancelPendingSignalsRunningTaskAndCancelsQueuedTask)
{
    auto               pool = create_pool(1, 1);
    std::promise<void> entered;
    auto               running = pool.submit([&](StopToken token) {
        entered.set_value();
        token.wait();
        return token.stop_requested();
    });
    ASSERT_TRUE(running.is_ok());
    ASSERT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    auto pending = pool.submit([] { return 9; });
    ASSERT_TRUE(pending.is_ok());
    auto running_future = std::move(running).unwrap();
    auto pending_future = std::move(pending).unwrap();

    EXPECT_TRUE(pool.shutdown(ShutdownMode::CancelPending).is_ok());
    EXPECT_TRUE(pool.join().is_ok());
    EXPECT_TRUE(running_future.get());
    EXPECT_THROW(pending_future.get(), TaskCancelled);
}

TEST(ThreadPoolTest, DrainCanUpgradeToCancelPending)
{
    auto               pool = create_pool(1, 1);
    std::promise<void> entered;
    std::promise<void> release;
    auto               release_signal = release.get_future().share();
    auto               running        = pool.submit([&] {
        entered.set_value();
        release_signal.wait();
    });
    ASSERT_TRUE(running.is_ok());
    ASSERT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    auto pending = pool.submit([] { return 5; });
    ASSERT_TRUE(pending.is_ok());
    auto pending_future = std::move(pending).unwrap();

    EXPECT_TRUE(pool.shutdown(ShutdownMode::Drain).is_ok());
    EXPECT_TRUE(pool.shutdown(ShutdownMode::CancelPending).is_ok());
    release.set_value();
    EXPECT_TRUE(pool.join().is_ok());
    EXPECT_THROW(pending_future.get(), TaskCancelled);
}

TEST(ThreadPoolTest, RejectsSubmitAfterShutdownAndJoinRequiresShutdown)
{
    auto pool = create_pool(1, 1);

    auto premature_join = pool.join();
    EXPECT_TRUE(premature_join.is_err());
    EXPECT_EQ(premature_join.code(), ca::core::StatusCode::FAILED_PRECONDITION);
    EXPECT_TRUE(pool.shutdown().is_ok());

    auto submitted = pool.submit([] {});
    ASSERT_TRUE(submitted.is_err());
    EXPECT_EQ(submitted.unwrap_err().code(), ca::core::StatusCode::FAILED_PRECONDITION);
    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
    EXPECT_TRUE(pool.is_shutdown());
    EXPECT_TRUE(pool.is_joined());
}

TEST(ThreadPoolTest, WorkerCannotJoinItsOwnPool)
{
    auto               pool = create_pool(1, 1);
    std::promise<void> may_join;
    auto               may_join_signal = may_join.get_future().share();
    auto               submitted       = pool.submit([&] {
        may_join_signal.wait();
        return pool.join();
    });
    ASSERT_TRUE(submitted.is_ok());
    auto future = std::move(submitted).unwrap();

    EXPECT_TRUE(pool.shutdown().is_ok());
    may_join.set_value();
    auto worker_join = future.get();
    ASSERT_TRUE(worker_join.is_err());
    EXPECT_EQ(worker_join.code(), ca::core::StatusCode::FAILED_PRECONDITION);
    EXPECT_FALSE(pool.is_joined());
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, ShutdownWakesSubmitBlockedByBackpressure)
{
    auto               pool = create_pool(1, 1);
    std::promise<void> entered;
    std::promise<void> release;
    auto               release_signal = release.get_future().share();
    auto               running        = pool.submit([&] {
        entered.set_value();
        release_signal.wait();
    });
    ASSERT_TRUE(running.is_ok());
    ASSERT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    ASSERT_TRUE(pool.submit([] {}).is_ok());

    auto blocked = std::async(std::launch::async, [&] { return pool.submit([] {}); });
    EXPECT_EQ(blocked.wait_for(20ms), std::future_status::timeout);
    EXPECT_TRUE(pool.shutdown(ShutdownMode::CancelPending).is_ok());
    ASSERT_EQ(blocked.wait_for(1s), std::future_status::ready);
    auto result = blocked.get();
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code(), ca::core::StatusCode::FAILED_PRECONDITION);
    release.set_value();
    EXPECT_TRUE(pool.join().is_ok());
}

TEST(ThreadPoolTest, DestructorDrainsAcceptedTasks)
{
    std::future<int> future;
    {
        auto pool   = create_pool(1, 1);
        auto result = pool.submit([] { return 11; });
        ASSERT_TRUE(result.is_ok());
        future = std::move(result).unwrap();
    }

    EXPECT_EQ(future.get(), 11);
}

TEST(ThreadPoolTest, MoveAssignmentDrainsPreviousPoolBeforeTakingOwnership)
{
    auto               destination = create_pool(1, 1);
    std::promise<void> entered;
    std::promise<void> release;
    auto               release_signal = release.get_future().share();
    auto               previous       = destination.submit([&] {
        entered.set_value();
        release_signal.wait();
        return 1;
    });
    ASSERT_TRUE(previous.is_ok());
    ASSERT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    auto previous_future = std::move(previous).unwrap();

    auto source   = create_pool(2, 2);
    auto assigned = std::async(std::launch::async, [&] { destination = std::move(source); });
    EXPECT_EQ(assigned.wait_for(20ms), std::future_status::timeout);
    release.set_value();
    ASSERT_EQ(assigned.wait_for(1s), std::future_status::ready);
    assigned.get();

    EXPECT_EQ(previous_future.get(), 1);
    EXPECT_EQ(destination.worker_count(), 2U);
    EXPECT_EQ(source.worker_count(), 0U);
    EXPECT_TRUE(destination.shutdown().is_ok());
    EXPECT_TRUE(destination.join().is_ok());
}

TEST(ThreadPoolTest, PoolCanBeReleasedByItsOwnTask)
{
    auto                      owner      = std::make_shared<ThreadPool>(create_pool(1, 1));
    std::weak_ptr<ThreadPool> weak_owner = owner;
    std::promise<void>        release;
    auto                      release_signal = release.get_future().share();
    auto                      submitted      = owner->submit([kept_alive = owner, release_signal] {
        release_signal.wait();
        return kept_alive->worker_count();
    });
    ASSERT_TRUE(submitted.is_ok());
    auto future = std::move(submitted).unwrap();

    owner.reset();
    EXPECT_FALSE(weak_owner.expired());
    release.set_value();
    EXPECT_EQ(future.get(), 1U);
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!weak_owner.expired() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    EXPECT_TRUE(weak_owner.expired());
}

TEST(ThreadPoolTest, SupportsConcurrentSubmitters)
{
    auto                                       pool               = create_pool(4, 8);
    constexpr int                              producer_count     = 4;
    constexpr int                              tasks_per_producer = 50;
    std::vector<std::vector<std::future<int>>> futures(producer_count);
    std::vector<std::thread>                   producers;
    for (int producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            for (int value = 1; value <= tasks_per_producer; ++value) {
                auto submitted = pool.submit([producer, value, tasks_per_producer] {
                    return producer * tasks_per_producer + value;
                });
                if (submitted.is_ok())
                    futures[producer].push_back(std::move(submitted).unwrap());
            }
        });
    }
    for (auto& producer : producers)
        producer.join();

    EXPECT_TRUE(pool.shutdown().is_ok());
    EXPECT_TRUE(pool.join().is_ok());
    int sum = 0;
    for (auto& producer_futures : futures) {
        ASSERT_EQ(producer_futures.size(), static_cast<usize>(tasks_per_producer));
        for (auto& future : producer_futures)
            sum += future.get();
    }
    EXPECT_EQ(sum, 20100);
}

}   // namespace
}   // namespace ca::thread::test
