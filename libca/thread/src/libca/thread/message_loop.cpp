#include "libca/thread/message_loop.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>

#include "libca/thread/thread.hpp"

namespace ca::thread {
namespace {

enum class LoopPhase
{
    Running,
    Stopping,
    Joined
};

enum class LoopStopMode
{
    Discard,
    Drain
};

ca::core::Status empty_loop_error(const char* operation)
{
    return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                               std::string(operation) + " on a moved MessageLoop");
}

ca::core::Status stopped_loop_error()
{
    return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                               "message loop is stopping down");
}

// 调度线程、Impl 与停止路径共享的状态；除任务执行外全部字段只在 mutex 保护下访问。
class LoopState
{
public:
    mutable std::mutex      mutex;
    std::condition_variable wakeup;
    // 即时任务 FIFO 队列；延迟任务到期后也转投到这里，保证所有任务同队列串行。
    std::deque<std::function<void()>> ready;
    // 未到期延迟任务，按 (到期时间, 序号) 排序；序号保证同到期时间有稳定全序。
    struct DelayedTask
    {
        std::chrono::steady_clock::time_point due{};
        u64                                   seq{0};
        std::function<void()>                 run;
    };
    struct DelayedTaskOrder
    {
        bool operator()(const DelayedTask& lhs, const DelayedTask& rhs) const
        {
            if (lhs.due != rhs.due)
                return lhs.due < rhs.due;
            return lhs.seq < rhs.seq;
        }
    };
    std::set<DelayedTask, DelayedTaskOrder> delayed;
    u64                                     next_seq{1};
    StopSource                              stop_source;
    LoopPhase                               phase{LoopPhase::Running};
    LoopStopMode                            stop_mode{LoopStopMode::Discard};
};

// 工作线程主循环：持锁搬运/出队，任务一律在锁外执行，异常不逃出线程。
void run_loop(const std::shared_ptr<LoopState>& state)
{
    std::unique_lock<std::mutex> lock(state->mutex);
    for (;;) {
        // 到期延迟任务转投 ready 队尾：既保证线程亲和，也维持同队列 FIFO。
        if (state->phase == LoopPhase::Running && !state->delayed.empty()) {
            const auto now = std::chrono::steady_clock::now();
            while (!state->delayed.empty() && state->delayed.begin()->due <= now) {
                state->ready.push_back(std::move(state->delayed.begin()->run));
                state->delayed.erase(state->delayed.begin());
            }
        }
        if (!state->ready.empty()) {
            auto task = std::move(state->ready.front());
            state->ready.pop_front();
            lock.unlock();
            try {
                task();
            }
            catch (...) {
                // 异常隔离：fire-and-forget 任务的异常无处安放，吞掉以保住循环。
            }
            lock.lock();
            continue;
        }
        // 已无即时任务：丢弃与排空在这一点上等价（排空时延迟任务已在停止处废弃）。
        if (state->phase != LoopPhase::Running)
            return;
        if (state->delayed.empty()) {
            state->wakeup.wait(lock, [&] {
                return state->phase != LoopPhase::Running || !state->ready.empty();
            });
        }
        else {
            // 睡到最近到期点；队首变化（新更早任务、队首被取走）时提前醒来重算。
            const auto next_due = state->delayed.begin()->due;
            state->wakeup.wait_until(lock, next_due, [&] {
                return state->phase != LoopPhase::Running || !state->ready.empty() ||
                       state->delayed.empty() || state->delayed.begin()->due != next_due;
            });
        }
    }
}

}   // namespace

class MessageLoop::Impl
{
public:
    std::shared_ptr<LoopState> state;
    Thread                     worker;
    mutable std::mutex         join_mutex;
    ca::core::Status           join_status;
};

MessageLoop::MessageLoop(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{}

MessageLoop::MessageLoop(MessageLoop&& other) noexcept = default;

MessageLoop& MessageLoop::operator=(MessageLoop&& other) noexcept
{
    if (this != &other) {
        finish_noexcept();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

MessageLoop::~MessageLoop()
{
    finish_noexcept();
}

ca::core::StatusResult<MessageLoop> MessageLoop::create()
{
    try {
        auto         impl  = std::make_unique<Impl>();
        auto         state = std::make_shared<LoopState>();
        auto         started = Thread::start([state]() { run_loop(state); });
        if (started.is_err())
            return ca::core::Err(started.unwrap_err());
        impl->state  = std::move(state);
        impl->worker = std::move(started).unwrap();
        return ca::core::Ok(MessageLoop(std::move(impl)));
    }
    catch (const std::exception& error) {
        return ca::core::Err(
            ca::core::ErrStatus(ca::core::StatusCode::RESOURCE_EXHAUSTED,
                                std::string("message loop creation failed: ") + error.what()));
    }
    catch (...) {
        return ca::core::Err(
            ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                                "message loop creation failed with a non-standard exception"));
    }
}

ca::core::Status MessageLoop::enqueue(std::function<void()> task)
{
    if (impl_ == nullptr)
        return empty_loop_error("post_task");
    if (!task)
        return ca::core::ErrStatus(ca::core::StatusCode::INVALID_ARGUMENT,
                                   "message loop task must not be empty");
    auto state = impl_->state;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->phase != LoopPhase::Running)
            return stopped_loop_error();
        state->ready.push_back(std::move(task));
    }
    // 锁外 notify，与模块内其它同步点保持一致。
    state->wakeup.notify_one();
    return ca::core::OkStatus();
}

ca::core::Status MessageLoop::enqueue_delay(std::function<void()>          task,
                                            std::chrono::milliseconds delay)
{
    if (impl_ == nullptr)
        return empty_loop_error("post_delay_task");
    if (!task)
        return ca::core::ErrStatus(ca::core::StatusCode::INVALID_ARGUMENT,
                                   "message loop task must not be empty");
    auto state = impl_->state;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->phase != LoopPhase::Running)
            return stopped_loop_error();
        // 负延迟按 0 处理：尽快执行，与 TimerManager 一致。
        LoopState::DelayedTask entry;
        entry.due = std::chrono::steady_clock::now() +
                    (delay > std::chrono::milliseconds::zero() ? delay : std::chrono::milliseconds::zero());
        entry.seq = state->next_seq++;
        entry.run = std::move(task);
        state->delayed.insert(std::move(entry));
    }
    state->wakeup.notify_all();   // 新任务可能成为最近到期者，唤醒重算等待时长
    return ca::core::OkStatus();
}

ca::core::Status MessageLoop::stop()
{
    if (impl_ == nullptr)
        return empty_loop_error("stop");
    auto state = impl_->state;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->phase != LoopPhase::Running)
            return ca::core::OkStatus();   // 幂等
        state->phase     = LoopPhase::Stopping;
        state->stop_mode = LoopStopMode::Discard;
        // 丢弃语义：未执行任务（即时 + 延迟）全部就地清除。
        state->ready.clear();
        state->delayed.clear();
        state->stop_source.request_stop();
    }
    state->wakeup.notify_all();
    return ca::core::OkStatus();
}

ca::core::Status MessageLoop::stop_and_drain()
{
    if (impl_ == nullptr)
        return empty_loop_error("stop_and_drain");
    auto state = impl_->state;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->phase != LoopPhase::Running)
            return ca::core::OkStatus();   // 幂等
        state->phase     = LoopPhase::Stopping;
        state->stop_mode = LoopStopMode::Drain;
        // 排空语义：保留已入队即时任务让工作线程跑完；延迟任务（无论是否到期）废弃。
        state->delayed.clear();
        state->stop_source.request_stop();
    }
    state->wakeup.notify_all();
    return ca::core::OkStatus();
}

ca::core::Status MessageLoop::join()
{
    if (impl_ == nullptr)
        return empty_loop_error("join");

    // join_mutex 串行化多个控制线程的 join 调用。
    std::lock_guard<std::mutex> join_lock(impl_->join_mutex);
    {
        std::lock_guard<std::mutex> state_lock(impl_->state->mutex);
        if (impl_->state->phase == LoopPhase::Running)
            return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                       "stop must be called before join");
        if (impl_->state->phase == LoopPhase::Joined)
            return impl_->join_status;
    }
    // 禁止 loop 任务从循环线程内 join 自己的 loop，否则会死锁。
    if (impl_->worker.joinable() && impl_->worker.id() == std::this_thread::get_id())
        return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                   "the loop thread cannot join its own message loop");

    auto result = impl_->worker.join();
    {
        std::lock_guard<std::mutex> state_lock(impl_->state->mutex);
        impl_->join_status = result;
        impl_->state->phase = LoopPhase::Joined;
    }
    return result;
}

StopToken MessageLoop::stop_token() const noexcept
{
    return impl_ == nullptr ? StopToken{} : impl_->state->stop_source.token();
}

bool MessageLoop::running_on() const noexcept
{
    return impl_ != nullptr && impl_->worker.id() == std::this_thread::get_id();
}

bool MessageLoop::is_stopped() const noexcept
{
    if (impl_ == nullptr)
        return true;
    std::lock_guard<std::mutex> lock(impl_->state->mutex);
    return impl_->state->phase != LoopPhase::Running;
}

bool MessageLoop::is_joined() const noexcept
{
    if (impl_ == nullptr)
        return false;
    std::lock_guard<std::mutex> lock(impl_->state->mutex);
    return impl_->state->phase == LoopPhase::Joined;
}

usize MessageLoop::pending_task_count() const noexcept
{
    if (impl_ == nullptr)
        return 0;
    auto state = impl_->state;
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->ready.size() + state->delayed.size();
}

void MessageLoop::finish_noexcept() noexcept
{
    if (impl_ == nullptr)
        return;
    try {
        stop_and_drain();
        join();
    }
    catch (...) {
    }
}

}   // namespace ca::thread
