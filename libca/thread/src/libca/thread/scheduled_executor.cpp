#include "libca/thread/scheduled_executor.hpp"

#include <algorithm>
#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

namespace ca::thread {
namespace {

using Clock = ScheduledExecutor::Clock;
using Key   = std::pair<Clock::time_point, std::uint64_t>;

thread_local const details::ScheduleState* g_active_executor = nullptr;

ca::core::Status closed_error() {
    return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION, "scheduled executor is closed or moved");
}

ca::core::Status range_error() {
    return ca::core::ErrStatus(ca::core::StatusCode::OUT_OF_RANGE, "scheduled deadline exceeds steady_clock range");
}

bool add_delay(Clock::time_point base, Clock::duration delay, Clock::time_point& result) {
    const auto count = base.time_since_epoch().count();
    if (count > Clock::duration::max().count() - delay.count())
        return false;
    result = Clock::time_point(Clock::duration(count + delay.count()));
    return true;
}

ca::core::StatusResult<Clock::duration> convert_delay(std::chrono::milliseconds delay) {
    using Scale = std::ratio_divide<std::milli, Clock::period>;
    static_assert(Scale::den == 1, "steady_clock must represent whole milliseconds");
    if (delay <= std::chrono::milliseconds::zero())
        return ca::core::Ok(Clock::duration::zero());
    if (delay.count() > Clock::duration::max().count() / Scale::num)
        return ca::core::Err(range_error());
    return ca::core::Ok(Clock::duration(delay.count() * Scale::num));
}

}   // namespace

namespace details {

struct ScheduledEntry {
    PoolTask                                task;
    std::function<void(std::exception_ptr)> fail;
    StopSource                              stop;
    Key                                     key;
    Clock::duration                         period{};
    bool                                    fixed_rate{false};
    bool                                    running{false};
    bool                                    cancelled{false};
};

struct ScheduleState {
    using Queue = std::map<Key, std::shared_ptr<ScheduledEntry>>;
    std::mutex                                               mutex;
    std::condition_variable                                  changed;
    Queue                                                    queue;
    std::map<std::uint64_t, std::shared_ptr<ScheduledEntry>> entries;
    std::uint64_t                                            next_id{1};
    usize                                                    capacity{0};
    bool                                                     closed{false};
    ShutdownMode                                             mode{ShutdownMode::Drain};
    std::exception_ptr range_failure = std::make_exception_ptr(std::overflow_error("scheduled deadline overflow"));
};

}   // namespace details

namespace {

void worker_loop(const std::shared_ptr<details::ScheduleState>& state) {
    g_active_executor = state.get();
    for (;;) {
        details::ScheduleState::Queue::node_type node;
        std::shared_ptr<details::ScheduledEntry> entry;
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            for (;;) {
                if (state->closed && state->queue.empty()) {
                    g_active_executor = nullptr;
                    return;
                }
                if (state->queue.empty()) {
                    state->changed.wait(lock);
                    continue;
                }
                const auto deadline = state->queue.begin()->first.first;
                const auto now      = Clock::now();
                if (deadline > now) {
                    // 限制单次等待，避免条件变量后端把极远时间点转换为平台时长时溢出。
                    const auto distance     = static_cast<std::uint64_t>(deadline.time_since_epoch().count()) -
                                              static_cast<std::uint64_t>(now.time_since_epoch().count());
                    const auto maximum_wait = std::chrono::duration_cast<Clock::duration>(std::chrono::hours(24));
                    const auto wait =
                        Clock::duration(std::min(distance, static_cast<std::uint64_t>(maximum_wait.count())));
                    state->changed.wait_for(lock, wait);
                    continue;
                }
                node           = state->queue.extract(state->queue.begin());
                entry          = node.mapped();
                entry->running = true;
                break;
            }
        }

        std::exception_ptr failure;
        try {
            entry->task.run();
        } catch (...) {
            failure = std::current_exception();
        }
        const auto finished = Clock::now();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            entry->running = false;
            if (entry->period == Clock::duration::zero()) {
                state->entries.erase(entry->key.second);
            } else if (failure) {
                entry->fail(failure);
                state->entries.erase(entry->key.second);
            } else if (entry->cancelled || state->closed) {
                entry->task.cancel();
                state->entries.erase(entry->key.second);
            } else {
                auto delay = entry->period;
                if (entry->fixed_rate) {
                    const auto elapsed = static_cast<std::uint64_t>(finished.time_since_epoch().count()) -
                                         static_cast<std::uint64_t>(entry->key.first.time_since_epoch().count());
                    delay -= Clock::duration(elapsed % static_cast<std::uint64_t>(delay.count()));
                }
                Clock::time_point next;
                if (add_delay(finished, delay, next)) {
                    entry->key.first = next;
                    node.key()       = entry->key;
                    state->queue.insert(std::move(node));
                } else {
                    entry->fail(state->range_failure);
                    state->entries.erase(entry->key.second);
                }
            }
        }
        state->changed.notify_all();
        // node 和 entry 在锁外释放，callable 析构允许重入执行器。
    }
}

}   // namespace

class ScheduledExecutor::Impl {
public:
    std::shared_ptr<details::ScheduleState> state = std::make_shared<details::ScheduleState>();
    std::vector<std::thread>                workers;
    std::mutex                              join_mutex;
};

ScheduledHandle::ScheduledHandle(std::weak_ptr<details::ScheduleState> state, std::uint64_t id) noexcept
    : state_(std::move(state))
    , id_(id) {}

bool ScheduledHandle::cancel() const {
    auto state = state_.lock();
    if (!state)
        return false;
    std::shared_ptr<details::ScheduledEntry> entry;
    details::ScheduleState::Queue::node_type node;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        auto                        found = state->entries.find(id_);
        if (found == state->entries.end() || found->second->cancelled)
            return false;
        entry            = found->second;
        entry->cancelled = true;
        entry->stop.request_stop();
        if (!entry->running) {
            node = state->queue.extract(entry->key);
            entry->task.cancel();
            state->entries.erase(found);
        }
    }
    state->changed.notify_all();
    return true;
}

ScheduledExecutor::ScheduledExecutor(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
ScheduledExecutor::ScheduledExecutor(ScheduledExecutor&& other) noexcept = default;

ScheduledExecutor& ScheduledExecutor::operator=(ScheduledExecutor&& other) noexcept {
    if (this != &other) {
        finish_noexcept();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

ScheduledExecutor::~ScheduledExecutor() {
    finish_noexcept();
}

ca::core::StatusResult<ScheduledExecutor> ScheduledExecutor::create(const ScheduledExecutorOptions& options) {
    if (options.task_capacity == 0)
        return ca::core::Err(
            ca::core::ErrStatus(ca::core::StatusCode::INVALID_ARGUMENT, "scheduled task capacity must be nonzero"));
    try {
        auto impl             = std::make_unique<Impl>();
        impl->state->capacity = options.task_capacity;
        auto count            = options.thread_count;
        if (count == 0)
            count = static_cast<usize>(std::thread::hardware_concurrency());
        if (count == 0)
            count = 1;
        impl->workers.reserve(count);
        ScheduledExecutor executor(std::move(impl));
        for (usize index = 0; index < count; ++index) {
            auto state = executor.impl_->state;
            executor.impl_->workers.emplace_back([state]() { worker_loop(state); });
        }
        return ca::core::Ok(std::move(executor));
    } catch (const std::exception& error) {
        return ca::core::Err(ca::core::ErrStatus(ca::core::StatusCode::RESOURCE_EXHAUSTED,
                                                 std::string("scheduled executor creation failed: ") + error.what()));
    } catch (...) {
        return ca::core::Err(details::task_setup_error());
    }
}

ca::core::StatusResult<ScheduledExecutor::Clock::time_point> ScheduledExecutor::deadline_after(
    std::chrono::milliseconds delay) {
    auto converted = convert_delay(delay);
    if (converted.is_err())
        return ca::core::Err(converted.unwrap_err());
    Clock::time_point deadline;
    if (!add_delay(Clock::now(), converted.unwrap(), deadline))
        return ca::core::Err(range_error());
    return ca::core::Ok(deadline);
}

ca::core::StatusResult<ScheduledHandle> ScheduledExecutor::enqueue(details::PoolTask                       task,
                                                                   std::function<void(std::exception_ptr)> fail,
                                                                   StopSource source, Clock::time_point deadline,
                                                                   std::chrono::milliseconds period, RepeatMode mode) {
    if (!impl_)
        return ca::core::Err(closed_error());
    auto converted = convert_delay(period);
    if (converted.is_err())
        return ca::core::Err(converted.unwrap_err());
    auto entry        = std::make_shared<details::ScheduledEntry>();
    entry->task       = std::move(task);
    entry->fail       = std::move(fail);
    entry->stop       = std::move(source);
    entry->period     = converted.unwrap();
    entry->fixed_rate = mode == RepeatMode::FixedRate;
    const auto& state = impl_->state;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->closed)
            return ca::core::Err(closed_error());
        if (state->entries.size() == state->capacity || state->next_id == std::numeric_limits<std::uint64_t>::max())
            return ca::core::Err(
                ca::core::ErrStatus(ca::core::StatusCode::RESOURCE_EXHAUSTED, "scheduled task capacity exhausted"));
        entry->key = Key(deadline, state->next_id++);
        state->entries.emplace(entry->key.second, entry);
        try {
            state->queue.emplace(entry->key, entry);
        } catch (...) {
            state->entries.erase(entry->key.second);
            throw;
        }
    }
    state->changed.notify_all();
    return ca::core::Ok(ScheduledHandle(state, entry->key.second));
}

ca::core::Status ScheduledExecutor::shutdown(ShutdownMode mode) {
    if (!impl_)
        return closed_error();
    const auto&                   state = impl_->state;
    details::ScheduleState::Queue discarded;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->closed = true;
        if (mode == ShutdownMode::CancelPending)
            state->mode = mode;
        for (auto found = state->entries.begin(); found != state->entries.end();) {
            const auto& entry  = found->second;
            const bool  cancel = entry->period != Clock::duration::zero() || state->mode == ShutdownMode::CancelPending;
            if (!cancel) {
                ++found;
                continue;
            }
            entry->cancelled = true;
            if (state->mode == ShutdownMode::CancelPending)
                entry->stop.request_stop();
            if (entry->running) {
                ++found;
                continue;
            }
            discarded.insert(state->queue.extract(entry->key));
            entry->task.cancel();
            found = state->entries.erase(found);
        }
    }
    state->changed.notify_all();
    return ca::core::OkStatus();
}

ca::core::Status ScheduledExecutor::join() {
    if (!impl_)
        return closed_error();
    const auto& state = impl_->state;
    if (g_active_executor == state.get())
        return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                   "a worker cannot join its scheduled executor");
    std::lock_guard<std::mutex> join_lock(impl_->join_mutex);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->closed)
            return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                       "shutdown must be called before join");
    }
    try {
        for (auto& worker : impl_->workers)
            if (worker.joinable())
                worker.join();
    } catch (const std::system_error& error) {
        return ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                                   std::string("scheduled worker join failed: ") + error.what());
    }
    return ca::core::OkStatus();
}

usize ScheduledExecutor::task_count() const {
    if (!impl_)
        return 0;
    std::lock_guard<std::mutex> lock(impl_->state->mutex);
    return impl_->state->entries.size();
}

usize ScheduledExecutor::worker_count() const noexcept {
    return impl_ ? impl_->workers.size() : 0;
}

bool ScheduledExecutor::is_shutdown() const {
    if (!impl_)
        return true;
    std::lock_guard<std::mutex> lock(impl_->state->mutex);
    return impl_->state->closed;
}

void ScheduledExecutor::finish_noexcept() noexcept {
    if (!impl_)
        return;
    try {
        shutdown(ShutdownMode::CancelPending);
        if (g_active_executor == impl_->state.get()) {
            for (auto& worker : impl_->workers)
                if (worker.joinable())
                    worker.detach();
        } else {
            join();
        }
    } catch (...) {
        for (auto& worker : impl_->workers)
            if (worker.joinable())
                worker.detach();
    }
}

}   // namespace ca::thread
