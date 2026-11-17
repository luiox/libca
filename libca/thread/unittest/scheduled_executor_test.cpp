#include <gtest/gtest.h>

#include "libca/thread/scheduled_executor.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace ca::thread {
namespace {

using namespace std::chrono_literals;
using Clock = ScheduledExecutor::Clock;

ScheduledExecutor make_executor(usize workers = 1, usize capacity = 32) {
    auto created = ScheduledExecutor::create({workers, capacity});
    return std::move(created).unwrap();
}

template<typename Result>
ScheduledTask<Result> accepted(ca::core::StatusResult<ScheduledTask<Result>> result) {
    return std::move(result).unwrap();
}

}   // namespace

TEST(ScheduledExecutorTest, RejectsInvalidCapacityAndRequiresShutdownBeforeJoin) {
    auto invalid = ScheduledExecutor::create({1, 0});
    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().code(), ca::core::StatusCode::INVALID_ARGUMENT);
    auto executor = make_executor();
    EXPECT_EQ(executor.join().code(), ca::core::StatusCode::FAILED_PRECONDITION);
    EXPECT_TRUE(executor.shutdown().is_ok());
    EXPECT_TRUE(executor.join().is_ok());
    EXPECT_TRUE(executor.join().is_ok());
}

TEST(ScheduledExecutorTest, DoesNotExecuteEarlyAndWakesForAnEarlierDeadline) {
    auto       executor = make_executor();
    auto       late     = accepted(executor.schedule_after(24h, [] { return 1; }));
    const auto deadline = Clock::now() + 30ms;
    auto       early    = accepted(executor.schedule_at(deadline, [] { return Clock::now(); }));
    ASSERT_EQ(early.future.wait_for(2s), std::future_status::ready);
    EXPECT_GE(early.future.get(), deadline);
    EXPECT_EQ(late.future.wait_for(0ms), std::future_status::timeout);
    EXPECT_TRUE(late.handle.cancel());
    EXPECT_THROW(late.future.get(), TaskCancelled);
}

TEST(ScheduledExecutorTest, EqualDeadlinesUseAdmissionOrderWithOneWorker) {
    auto                             executor = make_executor();
    std::vector<int>                 order;
    std::vector<ScheduledTask<void>> tasks;
    const auto                       deadline = Clock::now() + 30ms;
    for (int value = 0; value < 8; ++value)
        tasks.push_back(accepted(executor.schedule_at(deadline, [&order, value] { order.push_back(value); })));
    executor.shutdown(ShutdownMode::Drain);
    ASSERT_TRUE(executor.join().is_ok());
    for (auto& task : tasks)
        task.future.get();
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST(ScheduledExecutorTest, HandlesPastAndNegativeDelaysAndMoveOnlyResults) {
    auto executor  = make_executor();
    auto immediate = accepted(
        executor.schedule_after(-1ms, [value = std::make_unique<int>(42)]() mutable { return std::move(value); }));
    auto past = accepted(executor.schedule_at(Clock::time_point::min(), [] { return 7; }));
    ASSERT_EQ(immediate.future.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(*immediate.future.get(), 42);
    EXPECT_EQ(past.future.get(), 7);
}

TEST(ScheduledExecutorTest, RejectsOverflowAndInvalidPeriods) {
    auto executor = make_executor();
    auto overflow = executor.schedule_after(std::chrono::milliseconds::max(), [] {});
    ASSERT_TRUE(overflow.is_err());
    EXPECT_EQ(overflow.unwrap_err().code(), ca::core::StatusCode::OUT_OF_RANGE);
    EXPECT_TRUE(executor.schedule_fixed_delay(0ms, 0ms, [] {}).is_err());
    EXPECT_TRUE(executor.schedule_fixed_rate(0ms, -1ms, [] {}).is_err());
    auto huge_period = executor.schedule_fixed_rate(0ms, std::chrono::milliseconds::max(), [] {});
    ASSERT_TRUE(huge_period.is_err());
    EXPECT_EQ(huge_period.unwrap_err().code(), ca::core::StatusCode::OUT_OF_RANGE);
}

TEST(ScheduledExecutorTest, CancellationFreesCapacityAndIsIdempotent) {
    auto executor = make_executor(1, 1);
    auto waiting  = accepted(executor.schedule_at(Clock::time_point::max(), [] {}));
    auto full     = executor.schedule_after(0ms, [] {});
    ASSERT_TRUE(full.is_err());
    EXPECT_EQ(full.unwrap_err().code(), ca::core::StatusCode::RESOURCE_EXHAUSTED);
    const auto copy = waiting.handle;
    EXPECT_TRUE(copy.cancel());
    EXPECT_FALSE(waiting.handle.cancel());
    EXPECT_EQ(executor.task_count(), 0u);
    EXPECT_THROW(waiting.future.get(), TaskCancelled);
    auto replacement = accepted(executor.schedule_after(0ms, [] { return 9; }));
    EXPECT_EQ(replacement.future.get(), 9);
}

TEST(ScheduledExecutorTest, CancelQueuedTaskCannotRunAfterCapacityBecomesAvailable) {
    auto               executor = make_executor(1, 2);
    std::promise<void> running;
    auto               running_future = running.get_future();
    auto               blocker        = accepted(executor.schedule_after(0ms, [&running](StopToken token) {
        running.set_value();
        token.wait();
    }));
    ASSERT_EQ(running_future.wait_for(2s), std::future_status::ready);
    std::atomic<bool> called{false};
    auto              queued = accepted(executor.schedule_after(0ms, [&called] { called = true; }));
    EXPECT_TRUE(queued.handle.cancel());
    EXPECT_THROW(queued.future.get(), TaskCancelled);
    EXPECT_TRUE(blocker.handle.cancel());
    blocker.future.get();
    EXPECT_FALSE(called);
}

TEST(ScheduledExecutorTest, RunningCancellationIsCooperativeAndPreservesResult) {
    auto               executor = make_executor();
    std::promise<void> started;
    auto               started_future = started.get_future();
    auto               running        = accepted(executor.schedule_after(0ms, [&started](StopToken token) {
        started.set_value();
        token.wait();
        return 17;
    }));
    ASSERT_EQ(started_future.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(running.handle.cancel());
    ASSERT_EQ(running.future.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(running.future.get(), 17);
}

TEST(ScheduledExecutorTest, TaskExceptionDoesNotStopWorkersAndReleasesCapacity) {
    auto executor = make_executor(1, 1);
    auto failed   = accepted(executor.schedule_after(0ms, []() -> int { throw std::logic_error("task failure"); }));
    EXPECT_THROW(failed.future.get(), std::logic_error);
    const auto deadline = Clock::now() + 2s;
    while (executor.task_count() != 0 && Clock::now() < deadline)
        std::this_thread::yield();
    auto next = executor.schedule_after(0ms, [] { return 31; });
    ASSERT_TRUE(next.is_ok());
    EXPECT_EQ(std::move(next).unwrap().future.get(), 31);
    executor.shutdown(ShutdownMode::Drain);
    ASSERT_TRUE(executor.join().is_ok());
    EXPECT_EQ(executor.task_count(), 0u);
}

TEST(ScheduledExecutorTest, FixedDelayWaitsFromCompletionAndNeverOverlaps) {
    auto                           executor = make_executor(3);
    std::mutex                     mutex;
    std::condition_variable        changed;
    std::vector<Clock::time_point> starts;
    std::vector<Clock::time_point> finishes;
    auto                           task = accepted(executor.schedule_fixed_delay(0ms, 15ms, [&] {
        std::lock_guard<std::mutex> lock(mutex);
        starts.push_back(Clock::now());
        finishes.push_back(Clock::now());
        changed.notify_all();
    }));
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(changed.wait_for(lock, 2s, [&] { return starts.size() >= 3; }));
    }
    EXPECT_TRUE(task.handle.cancel());
    EXPECT_THROW(task.future.get(), TaskCancelled);
    std::lock_guard<std::mutex> lock(mutex);
    for (usize index = 1; index < starts.size(); ++index)
        EXPECT_GE(starts[index] - finishes[index - 1], 15ms);
}

TEST(ScheduledExecutorTest, FixedRateSkipsMissedSlotsAndDoesNotOverlap) {
    auto                            executor = make_executor(3);
    constexpr auto                  PERIOD   = 50ms;
    std::atomic<int>                active{0};
    std::atomic<bool>               overlap{false};
    std::atomic<int>                calls{0};
    std::promise<void>              release;
    auto                            release_future = release.get_future();
    std::promise<Clock::time_point> first_finished;
    auto                            first_finished_future = first_finished.get_future();
    std::promise<Clock::time_point> second_started;
    auto                            second_started_future = second_started.get_future();
    const auto                      scheduled_before      = Clock::now();
    auto                            task = accepted(executor.schedule_fixed_rate(0ms, PERIOD, [&](StopToken token) {
        if (active.fetch_add(1) != 0)
            overlap = true;
        const int number = ++calls;
        if (number == 1) {
            release_future.wait_for(2s);
            first_finished.set_value(Clock::now());
        }
        if (number == 2) {
            second_started.set_value(Clock::now());
            token.wait();
        }
        --active;
    }));
    const auto                      scheduled_after = Clock::now();
    // 将首轮阻塞到多个周期以后，在时间网格中间释放，触发跳过过期槽位的路径。
    const auto release_at = scheduled_after + 125ms;
    while (Clock::now() < release_at)
        std::this_thread::sleep_until(release_at);
    release.set_value();
    ASSERT_EQ(first_finished_future.wait_for(2s), std::future_status::ready);
    const auto finished = first_finished_future.get();
    ASSERT_EQ(second_started_future.wait_for(2s), std::future_status::ready);
    const auto started       = second_started_future.get();
    const auto earliest_slot = (finished - scheduled_before) / PERIOD;
    const auto latest_slot   = (finished - scheduled_after) / PERIOD;
    if (earliest_slot == latest_slot) {
        const auto earliest_next_tick = scheduled_before + PERIOD * (earliest_slot + 1);
        EXPECT_GE(started, earliest_next_tick);
    }
    EXPECT_GT(started, finished);
    EXPECT_TRUE(task.handle.cancel());
    EXPECT_THROW(task.future.get(), TaskCancelled);
    EXPECT_FALSE(overlap);
    EXPECT_EQ(calls.load(), 2);
}

TEST(ScheduledExecutorTest, PeriodicExceptionTerminatesFutureAndFreesRegistration) {
    auto executor = make_executor(1, 1);
    auto task     = accepted(executor.schedule_fixed_delay(0ms, 1ms, [] { throw std::runtime_error("repeat"); }));
    EXPECT_THROW(task.future.get(), std::runtime_error);
    executor.shutdown(ShutdownMode::Drain);
    EXPECT_TRUE(executor.join().is_ok());
    EXPECT_EQ(executor.task_count(), 0u);
}

TEST(ScheduledExecutorTest, DrainPreservesOneShotDeadlinesAndStopsPeriodicTasks) {
    auto       executor = make_executor();
    const auto deadline = Clock::now() + 30ms;
    auto       once     = accepted(executor.schedule_at(deadline, [] { return Clock::now(); }));
    auto       repeated = accepted(executor.schedule_fixed_delay(24h, 1ms, [] {}));
    EXPECT_TRUE(executor.shutdown(ShutdownMode::Drain).is_ok());
    EXPECT_TRUE(executor.schedule_after(0ms, [] {}).is_err());
    ASSERT_TRUE(executor.join().is_ok());
    EXPECT_GE(once.future.get(), deadline);
    EXPECT_THROW(repeated.future.get(), TaskCancelled);
}

TEST(ScheduledExecutorTest, CancelPendingUpgradesDrainAndAllFuturesBecomeTerminal) {
    auto executor = make_executor();
    auto delayed  = accepted(executor.schedule_after(24h, [] {}));
    executor.shutdown(ShutdownMode::Drain);
    EXPECT_TRUE(executor.shutdown(ShutdownMode::CancelPending).is_ok());
    EXPECT_TRUE(executor.join().is_ok());
    EXPECT_EQ(delayed.future.wait_for(0ms), std::future_status::ready);
    EXPECT_THROW(delayed.future.get(), TaskCancelled);
}

TEST(ScheduledExecutorTest, WorkerJoinRejectsEvenWhileExternalJoinWaits) {
    auto               executor = make_executor();
    std::promise<void> entered;
    auto               entered_future = entered.get_future();
    auto               task           = accepted(executor.schedule_after(0ms, [&](StopToken token) {
        entered.set_value();
        token.wait();
        return executor.join().code();
    }));
    ASSERT_EQ(entered_future.wait_for(2s), std::future_status::ready);
    auto joiner = std::async(std::launch::async, [&] {
        executor.shutdown();
        return executor.join();
    });
    ASSERT_EQ(task.future.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(task.future.get(), ca::core::StatusCode::FAILED_PRECONDITION);
    EXPECT_TRUE(joiner.get().is_ok());
}

TEST(ScheduledExecutorTest, MovePreservesTasksAndDestructionCancelsPendingFutures) {
    std::future<void> future;
    ScheduledHandle   handle;
    {
        auto original = make_executor();
        auto task     = accepted(original.schedule_after(24h, [] {}));
        future        = std::move(task.future);
        handle        = task.handle;
        auto moved    = std::move(original);
        EXPECT_EQ(original.worker_count(), 0u);
        EXPECT_TRUE(original.schedule_after(0ms, [] {}).is_err());
        EXPECT_EQ(moved.task_count(), 1u);
    }
    EXPECT_EQ(future.wait_for(0ms), std::future_status::ready);
    EXPECT_THROW(future.get(), TaskCancelled);
    EXPECT_FALSE(handle.cancel());
}

TEST(ScheduledExecutorTest, ConcurrentCancelAndShutdownSettleEveryAcceptedFuture) {
    auto                             executor = make_executor(2, 64);
    std::vector<ScheduledTask<void>> tasks;
    for (int index = 0; index < 64; ++index)
        tasks.push_back(accepted(executor.schedule_after(24h, [] {})));
    auto cancelling = std::async(std::launch::async, [&] {
        for (const auto& task : tasks)
            task.handle.cancel();
    });
    executor.shutdown();
    EXPECT_TRUE(executor.join().is_ok());
    cancelling.get();
    for (auto& task : tasks) {
        ASSERT_EQ(task.future.wait_for(0ms), std::future_status::ready);
        EXPECT_THROW(task.future.get(), TaskCancelled);
    }
}

TEST(ScheduledExecutorTest, RunningPeriodicCancellationWaitsForCurrentInvocation) {
    auto               executor = make_executor(2);
    std::promise<void> entered;
    std::promise<void> observed;
    std::promise<void> release;
    auto               entered_future  = entered.get_future();
    auto               observed_future = observed.get_future();
    auto               release_future  = release.get_future();
    auto               task            = accepted(executor.schedule_fixed_delay(0ms, 1ms, [&](StopToken token) {
        entered.set_value();
        token.wait();
        observed.set_value();
        release_future.wait_for(2s);
    }));
    ASSERT_EQ(entered_future.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(task.handle.cancel());
    EXPECT_EQ(observed_future.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(task.future.wait_for(0ms), std::future_status::timeout);
    release.set_value();
    EXPECT_THROW(task.future.get(), TaskCancelled);
}

TEST(ScheduledExecutorTest, CallbackCanDestroyItsExecutorWithSharedWorkerState) {
    auto               owner = std::make_shared<ScheduledExecutor>(make_executor(2));
    std::promise<void> release;
    auto               release_future = release.get_future();
    auto               task           = accepted(owner->schedule_after(0ms, [owner, &release_future]() mutable {
        release_future.wait_for(2s);
        owner.reset();
        return 11;
    }));
    owner.reset();
    release.set_value();
    ASSERT_EQ(task.future.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(task.future.get(), 11);
}

TEST(ScheduledExecutorTest, CallableDestructorCanReenterExecutor) {
    auto executor = make_executor();
    struct ReentrantCapture {
        ReentrantCapture(ScheduledExecutor& owner, std::atomic<bool>& observed)
            : executor(&owner)
            , destroyed(&observed) {}
        ScheduledExecutor* executor;
        std::atomic<bool>* destroyed;
        ~ReentrantCapture() {
            executor->task_count();
            *destroyed = true;
        }
    };
    std::atomic<bool> destroyed{false};
    auto              capture = std::make_shared<ReentrantCapture>(executor, destroyed);
    auto task                 = accepted(executor.schedule_after(24h, [capture] {}));
    capture.reset();
    EXPECT_TRUE(task.handle.cancel());
    EXPECT_TRUE(destroyed);
    EXPECT_THROW(task.future.get(), TaskCancelled);
}

TEST(ScheduledExecutorTest, CallbackCanSubmitAndDrainItsExecutor) {
    auto executor = make_executor();
    auto outer    = accepted(executor.schedule_after(0ms, [&] {
        auto inner = accepted(executor.schedule_after(0ms, [] { return 12; }));
        executor.shutdown(ShutdownMode::Drain);
        return std::move(inner.future);
    }));
    ASSERT_EQ(outer.future.wait_for(2s), std::future_status::ready);
    auto inner_future = outer.future.get();
    EXPECT_TRUE(executor.join().is_ok());
    EXPECT_EQ(inner_future.get(), 12);
}

TEST(ScheduledExecutorTest, CancellingOneTaskDoesNotStopAnotherRegistration) {
    auto               executor = make_executor(2);
    std::promise<void> first_started;
    std::promise<void> second_started;
    auto               first_entered  = first_started.get_future();
    auto               second_entered = second_started.get_future();
    auto               first          = accepted(executor.schedule_after(0ms, [&](StopToken token) {
        first_started.set_value();
        token.wait();
    }));
    auto               second         = accepted(executor.schedule_after(0ms, [&](StopToken token) {
        second_started.set_value();
        token.wait();
    }));
    ASSERT_EQ(first_entered.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(second_entered.wait_for(2s), std::future_status::ready);
    first.handle.cancel();
    ASSERT_EQ(first.future.wait_for(2s), std::future_status::ready);
    EXPECT_NO_THROW(first.future.get());
    EXPECT_EQ(second.future.wait_for(0ms), std::future_status::timeout);
    second.handle.cancel();
    EXPECT_NO_THROW(second.future.get());
}

}   // namespace ca::thread
