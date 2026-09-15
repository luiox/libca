#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "libca/thread/message_loop.hpp"

namespace ca::thread::test {
namespace {

using namespace std::chrono_literals;

// 简单等待 future 到位，超时返回 false，避免测试因调度抖动而挂死。
template<typename Future>
bool wait_ready(const Future& future, std::chrono::milliseconds timeout)
{
    return future.wait_for(timeout) == std::future_status::ready;
}

// 轮询直到谓词成立或到达总截止时间；时序断言一律走该路径，禁止无界等待。
template<typename Predicate>
bool poll_until(Predicate&& predicate, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(2ms);
    }
    return predicate();
}

// 创建并启动一个 MessageLoop；create 只有线程资源枯竭才会失败，失败即记录断言。
MessageLoop make_loop()
{
    auto created = MessageLoop::create();
    EXPECT_TRUE(created.is_ok());
    return std::move(created).unwrap();
}

// 阻塞门任务：通知已开始，等待放行信号后返回；等待自带上限，防测试逻辑失效时挂死。
std::function<void()> make_gate(std::promise<void>& started, std::shared_future<void> release)
{
    return [&started, release] {
        started.set_value();
        release.wait_for(5s);
    };
}

TEST(MessageLoopTest, ExecutesTasksInFifoOrder)
{
    auto             loop = make_loop();
    std::vector<int> order;

    constexpr int kTaskCount = 200;
    for (int index = 0; index < kTaskCount; ++index)
        ASSERT_TRUE(loop.post_task([&order, index] { order.push_back(index); }).is_ok());

    // 序列屏障：marker 排在最后，marker 就位即代表此前任务全部完成。
    std::promise<void> drained;
    auto               drained_future = drained.get_future();
    ASSERT_TRUE(loop.post_task([&drained] { drained.set_value(); }).is_ok());
    ASSERT_TRUE(wait_ready(drained_future, 5s));

    ASSERT_EQ(order.size(), static_cast<usize>(kTaskCount));
    for (int index = 0; index < kTaskCount; ++index)
        EXPECT_EQ(order[static_cast<usize>(index)], index);
}

TEST(MessageLoopTest, RunningOnIdentifiesLoopThread)
{
    auto loop = make_loop();

    EXPECT_FALSE(loop.running_on());   // 主线程不是 loop 线程

    std::promise<bool> inside;
    auto               inside_future = inside.get_future().share();
    ASSERT_TRUE(loop.post_task([&loop, &inside] { inside.set_value(loop.running_on()); }).is_ok());
    ASSERT_TRUE(wait_ready(inside_future, 5s));
    EXPECT_TRUE(inside_future.get());
}

TEST(MessageLoopTest, PostTaskWithResultDeliversToCallerThread)
{
    auto loop = make_loop();

    const auto main_id = std::this_thread::get_id();
    auto       posted  = loop.post_task_with_result([main_id] {
        // fn 在 loop 线程执行：执行线程不是投递线程。
        return std::this_thread::get_id() != main_id;
    });
    ASSERT_TRUE(posted.is_ok());

    // 结果由调用方（主）线程取出：future.get() 在本线程完成“跨线程递交”。
    const bool ran_on_loop_thread = std::move(posted).unwrap().get();
    EXPECT_TRUE(ran_on_loop_thread);
}

TEST(MessageLoopTest, PostTaskWithResultPropagatesTaskException)
{
    auto loop = make_loop();

    auto posted = loop.post_task_with_result([]() -> int { throw std::runtime_error("boom"); });
    ASSERT_TRUE(posted.is_ok());

    EXPECT_THROW(std::move(posted).unwrap().get(), std::runtime_error);
}

TEST(MessageLoopTest, DelayTaskFiresAfterDeadlineOnLoopThread)
{
    auto loop = make_loop();

    const auto         main_id = std::this_thread::get_id();
    std::atomic<bool>  fired{false};
    std::atomic<bool>  on_other_thread{false};
    std::atomic<int64_t> actual_nanos{0};

    const auto posted_at = std::chrono::steady_clock::now();
    ASSERT_TRUE(loop.post_delay_task(
                    [&fired, &on_other_thread, &actual_nanos, main_id, posted_at] {
                        actual_nanos.store((std::chrono::steady_clock::now() - posted_at).count());
                        on_other_thread.store(std::this_thread::get_id() != main_id);
                        fired.store(true);
                    },
                    80ms)
                    .is_ok());

    // 轮询 + 总截止，不做无界等待。
    ASSERT_TRUE(poll_until([&fired] { return fired.load(); }, 5s));
    EXPECT_GE(actual_nanos.load(), std::chrono::duration_cast<std::chrono::nanoseconds>(80ms).count());   // 不早于 deadline 执行
    EXPECT_TRUE(on_other_thread.load());   // 在 loop 线程而非主线程执行
    EXPECT_FALSE(loop.running_on());       // 主线程视角仍是 false
}

TEST(MessageLoopTest, DelayTasksExecuteInDueOrder)
{
    auto loop = make_loop();

    // 后投递但更早到期者先执行：到期顺序决定执行顺序。
    std::atomic<int> sequence{0};
    std::atomic<int> first_order{0};
    std::promise<void> second_fired;
    auto               second_future = second_fired.get_future();

    ASSERT_TRUE(loop.post_delay_task(
                    [&sequence, &first_order] { first_order.store(sequence.fetch_add(1)); }, 150ms)
                    .is_ok());
    ASSERT_TRUE(loop.post_delay_task(
                    [&sequence, &second_fired] {
                        sequence.fetch_add(1);
                        second_fired.set_value();
                    },
                    50ms)
                    .is_ok());

    ASSERT_TRUE(wait_ready(second_future, 5s));
    ASSERT_TRUE(poll_until([&sequence] { return sequence.load() >= 2; }, 5s));
    EXPECT_EQ(first_order.load(), 1);   // 150ms 的任务排在 50ms 的任务之后
}

TEST(MessageLoopTest, StopDiscardsPendingTasks)
{
    auto loop = make_loop();

    std::promise<void>  gate_started;
    std::promise<void>  gate_release;
    const auto          release_future = gate_release.get_future().share();
    ASSERT_TRUE(loop.post_task(make_gate(gate_started, release_future)).is_ok());
    ASSERT_EQ(gate_started.get_future().wait_for(5s), std::future_status::ready);

    std::atomic<int> ran{0};
    for (int index = 0; index < 5; ++index)
        ASSERT_TRUE(loop.post_task([&ran] { ran.fetch_add(1); }).is_ok());
    ASSERT_TRUE(loop.post_delay_task([&ran] { ran.fetch_add(1); }, 50ms).is_ok());

    // 门任务占住工作线程，此刻 5 个即时任务 + 1 个延迟任务都在待执行队列。
    EXPECT_EQ(loop.pending_task_count(), 6u);

    ASSERT_TRUE(loop.stop().is_ok());
    EXPECT_TRUE(loop.is_stopped());
    ASSERT_TRUE(loop.stop().is_ok());   // 重复 stop 幂等

    gate_release.set_value();           // 放行正在执行的门任务
    ASSERT_TRUE(loop.join().is_ok());
    EXPECT_TRUE(loop.is_joined());

    EXPECT_EQ(ran.load(), 0);           // 未执行任务（即时 + 延迟）全部被丢弃
}

TEST(MessageLoopTest, StopAndDrainRunsQueuedTasksButDropsDelayTasks)
{
    auto loop = make_loop();

    std::promise<void> gate_started;
    std::promise<void> gate_release;
    const auto         release_future = gate_release.get_future().share();
    ASSERT_TRUE(loop.post_task(make_gate(gate_started, release_future)).is_ok());
    ASSERT_EQ(gate_started.get_future().wait_for(5s), std::future_status::ready);

    std::atomic<int> ran{0};
    for (int index = 0; index < 5; ++index)
        ASSERT_TRUE(loop.post_task([&ran] { ran.fetch_add(1); }).is_ok());
    // 50ms 的延迟任务在排空窗口内本可触发，若未被废弃则 ran 会达到 6。
    ASSERT_TRUE(loop.post_delay_task([&ran] { ran.fetch_add(1); }, 50ms).is_ok());

    ASSERT_TRUE(loop.stop_and_drain().is_ok());
    ASSERT_TRUE(loop.stop_and_drain().is_ok());   // 重复 stop_and_drain 幂等

    gate_release.set_value();
    ASSERT_TRUE(loop.join().is_ok());

    EXPECT_EQ(ran.load(), 5);   // 已入队即时任务全部跑完，延迟任务被废弃
}

TEST(MessageLoopTest, PostAfterStopIsRejected)
{
    auto loop = make_loop();
    ASSERT_TRUE(loop.stop().is_ok());

    EXPECT_TRUE(loop.post_task([] {}).is_err());
    EXPECT_TRUE(loop.post_delay_task([] {}, 10ms).is_err());
    auto posted = loop.post_task_with_result([] { return 1; });
    EXPECT_TRUE(posted.is_err());
    EXPECT_EQ(posted.unwrap_err().code(), ca::core::StatusCode::FAILED_PRECONDITION);
}

TEST(MessageLoopTest, StopTokenRequestedOnStop)
{
    auto loop  = make_loop();
    auto token = loop.stop_token();

    EXPECT_TRUE(token.stop_possible());
    EXPECT_FALSE(token.stop_requested());

    ASSERT_TRUE(loop.stop_and_drain().is_ok());
    EXPECT_TRUE(token.stop_requested());   // stop / stop_and_drain 均请求共享令牌
}

TEST(MessageLoopTest, TaskMayPostFurtherTasks)
{
    auto loop = make_loop();

    // 链式投递：任务内继续 post 走队列，不应递归调用栈溢出。
    constexpr int      kChainDepth = 1000;
    std::atomic<int>   depth{0};
    std::promise<void> finished;
    auto               finished_future = finished.get_future();

    std::function<void()> chain;
    chain = [&loop, &chain, &depth, &finished, kChainDepth] {
        if (depth.fetch_add(1) + 1 >= kChainDepth) {
            finished.set_value();
            return;
        }
        loop.post_task(chain);
    };
    ASSERT_TRUE(loop.post_task(chain).is_ok());

    ASSERT_TRUE(wait_ready(finished_future, 5s));
    EXPECT_EQ(depth.load(), kChainDepth);
}

TEST(MessageLoopTest, ConcurrentPostStress)
{
    auto loop = make_loop();

    constexpr int    kPosterCount    = 4;
    constexpr int    kPostsPerThread = 2500;
    std::atomic<int> total{0};

    // 总截止 5 秒：整个压力过程（投递 + 排空）必须在此内完成。
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    std::vector<std::thread> posters;
    posters.reserve(kPosterCount);
    for (int poster = 0; poster < kPosterCount; ++poster) {
        posters.emplace_back([&loop, &total, kPostsPerThread] {
            for (int index = 0; index < kPostsPerThread; ++index)
                loop.post_task([&total] { total.fetch_add(1, std::memory_order_relaxed); });
        });
    }
    for (auto& poster : posters)
        poster.join();
    ASSERT_LE(std::chrono::steady_clock::now(), deadline);

    std::promise<void> drained;
    auto               drained_future = drained.get_future();
    ASSERT_TRUE(loop.post_task([&drained] { drained.set_value(); }).is_ok());
    ASSERT_TRUE(wait_ready(drained_future, 5s));

    ASSERT_TRUE(loop.stop_and_drain().is_ok());
    ASSERT_TRUE(loop.join().is_ok());
    EXPECT_LE(std::chrono::steady_clock::now(), deadline);

    EXPECT_EQ(total.load(), kPosterCount * kPostsPerThread);   // 无丢失、无重复
}

TEST(MessageLoopTest, DestructorDrainsPendingTasks)
{
    std::atomic<int> ran{0};
    {
        auto loop = make_loop();
        for (int index = 0; index < 10; ++index)
            ASSERT_TRUE(loop.post_task([&ran] { ran.fetch_add(1); }).is_ok());
        // 不显式 stop：析构兜底执行 stop_and_drain + join，已入队任务应全部跑完。
    }
    EXPECT_EQ(ran.load(), 10);
}

TEST(MessageLoopTest, DestructorSafeWithoutTasks)
{
    {
        auto loop = make_loop();   // 创建后从未投递任务，直接析构不应挂死
    }
    SUCCEED();
}

TEST(MessageLoopTest, JoinRequiresStopFirst)
{
    auto loop = make_loop();

    auto status = loop.join();
    EXPECT_EQ(status.code(), ca::core::StatusCode::FAILED_PRECONDITION);

    ASSERT_TRUE(loop.stop().is_ok());
    ASSERT_TRUE(loop.join().is_ok());
    ASSERT_TRUE(loop.join().is_ok());   // 重复 join 幂等
}

TEST(MessageLoopTest, JoinFromLoopThreadFails)
{
    auto loop = make_loop();

    std::promise<bool> rejected;
    auto               rejected_future = rejected.get_future();
    ASSERT_TRUE(loop.post_task([&loop, &rejected] {
        // loop 线程内 join 自己必然死锁，应被拒绝而不是挂死。
        rejected.set_value(loop.join().is_err());
    }).is_ok());

    ASSERT_TRUE(wait_ready(rejected_future, 5s));
    EXPECT_TRUE(rejected_future.get());

    ASSERT_TRUE(loop.stop().is_ok());
    ASSERT_TRUE(loop.join().is_ok());
}

TEST(MessageLoopTest, MovedFromLoopRejectsPosts)
{
    auto target = make_loop();
    auto moved  = std::move(target);

    EXPECT_TRUE(target.post_task([] {}).is_err());   // 被移动对象不再可用
    EXPECT_FALSE(moved.post_task([] {}).is_err());
    EXPECT_TRUE(moved.stop().is_ok());
    ASSERT_TRUE(moved.join().is_ok());
}

TEST(MessageLoopTest, RejectsEmptyTask)
{
    auto loop = make_loop();

    std::function<void()> empty;
    auto                  status = loop.post_task(empty);
    EXPECT_EQ(status.code(), ca::core::StatusCode::INVALID_ARGUMENT);

    auto delay_status = loop.post_delay_task(empty, 10ms);
    EXPECT_EQ(delay_status.code(), ca::core::StatusCode::INVALID_ARGUMENT);

    ASSERT_TRUE(loop.stop().is_ok());
    ASSERT_TRUE(loop.join().is_ok());
}

}  // namespace
}  // namespace ca::thread::test
