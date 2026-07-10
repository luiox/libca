#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>

#include "libca/thread/thread.hpp"

namespace ca::thread::test {
namespace {

using namespace std::chrono_literals;

TEST(ThreadTest, RunsCallableWithoutStopTokenAndJoinsIdempotently)
{
    std::promise<void> completed;
    auto started = Thread::start([&]() { completed.set_value(); });
    ASSERT_TRUE(started.is_ok()) << started.unwrap_err().to_string();

    auto thread = std::move(started).unwrap();
    EXPECT_EQ(completed.get_future().wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(thread.joinable());
    EXPECT_TRUE(thread.join().is_ok());
    EXPECT_TRUE(thread.join().is_ok());
    EXPECT_FALSE(thread.joinable());
}

TEST(ThreadTest, StopTokenWakesThreadAndJoinWaitsForCompletion)
{
    std::promise<void> entered;
    std::atomic<bool>  observed_stop{false};
    auto started = Thread::start([&](StopToken token) {
        entered.set_value();
        token.wait();
        observed_stop.store(token.stop_requested(), std::memory_order_release);
    });
    ASSERT_TRUE(started.is_ok()) << started.unwrap_err().to_string();

    auto thread = std::move(started).unwrap();
    EXPECT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(thread.request_stop());
    EXPECT_TRUE(thread.join().is_ok());
    EXPECT_TRUE(observed_stop.load(std::memory_order_acquire));
}

TEST(ThreadTest, DestructorRequestsStopAndJoins)
{
    std::promise<void> entered;
    std::atomic<bool>  finished{false};
    {
        auto started = Thread::start([&](StopToken token) {
            entered.set_value();
            token.wait();
            finished.store(true, std::memory_order_release);
        });
        ASSERT_TRUE(started.is_ok()) << started.unwrap_err().to_string();
        auto thread = std::move(started).unwrap();
        EXPECT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    }

    EXPECT_TRUE(finished.load(std::memory_order_acquire));
}

TEST(ThreadTest, JoinReportsThreadExceptionWithoutTerminating)
{
    auto started = Thread::start([]() { throw std::runtime_error("worker failure"); });
    ASSERT_TRUE(started.is_ok()) << started.unwrap_err().to_string();

    auto thread = std::move(started).unwrap();
    auto status = thread.join();

    EXPECT_TRUE(status.is_err());
    EXPECT_EQ(status.code(), ca::core::StatusCode::INTERNAL);
    EXPECT_NE(status.message().find("worker failure"), std::string::npos);
    EXPECT_EQ(thread.join(), status);
}

TEST(ThreadTest, MoveTransfersOwnership)
{
    std::promise<void> entered;
    auto started = Thread::start([&](StopToken token) {
        entered.set_value();
        token.wait();
    });
    ASSERT_TRUE(started.is_ok()) << started.unwrap_err().to_string();

    Thread source = std::move(started).unwrap();
    Thread target = std::move(source);
    EXPECT_FALSE(source.joinable());
    EXPECT_TRUE(target.joinable());
    EXPECT_EQ(entered.get_future().wait_for(1s), std::future_status::ready);
    target.request_stop();
    EXPECT_TRUE(target.join().is_ok());
}

}   // namespace
}   // namespace ca::thread::test
