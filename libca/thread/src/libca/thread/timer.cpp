#include "libca/thread/timer.hpp"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <utility>

#include "libca/core/datatype.hpp"

namespace ca::thread {
namespace details {

// 定时器条目。expiry/id/callback/cancelled 均只在 TimerState::mutex 保护下访问；
// 已出队但正在执行回调的条目，其 cancelled 标志同样由该锁保护。
struct TimerEntry
{
    usize                                id{0};
    std::chrono::steady_clock::time_point expiry{};
    std::chrono::milliseconds            period{0};
    bool                                 repeating{false};
    bool                                 cancelled{false};
    // 统一为 bool() 签名：一次性任务被包装为固定返回 false，重复任务的返回值决定续期。
    std::function<bool()>                callback;
};

// 按 (到期时间, id) 排序；id 唯一保证同到期时间的条目也有稳定全序，可按 key 精确 erase。
struct TimerEntryComparator
{
    bool operator()(const std::shared_ptr<TimerEntry>& lhs, const std::shared_ptr<TimerEntry>& rhs) const
    {
        if (lhs->expiry != rhs->expiry)
            return lhs->expiry < rhs->expiry;
        return lhs->id < rhs->id;
    }
};

// 调度线程、管理器与句柄三方共享的状态。
class TimerState
{
public:
    mutable std::mutex      mutex;
    std::condition_variable condition;
    std::set<std::shared_ptr<TimerEntry>, TimerEntryComparator> timers;
    usize                   next_id{1};
    bool                    shutdown{false};
};

}  // namespace details

namespace {

// 调度线程主循环：持锁检查队首到期情况，回调一律在锁外执行，异常不逃出线程。
void run_scheduler_loop(const std::shared_ptr<details::TimerState>& state)
{
    std::unique_lock<std::mutex> lock(state->mutex);
    while (!state->shutdown) {
        if (state->timers.empty()) {
            state->condition.wait(lock, [&] { return state->shutdown || !state->timers.empty(); });
            continue;
        }
        auto current = *state->timers.begin();
        if (current->expiry > std::chrono::steady_clock::now()) {
            // 队首变化（插入更早任务、队首被取消或被触发）时谓词成立，提前醒来重算；
            // 否则睡到当前队首到期为止。
            state->condition.wait_until(lock, current->expiry, [&] {
                return state->shutdown || state->timers.empty() ||
                       state->timers.begin()->get() != current.get();
            });
            continue;
        }
        // 到期：出队并快照取消状态，然后锁外执行回调。取消与触发的线性化点在此处出队。
        state->timers.erase(state->timers.begin());
        const bool cancelled = current->cancelled;
        lock.unlock();
        bool reschedule = false;
        if (!cancelled) {
            try {
                reschedule = current->callback();
            }
            catch (...) {
                // 异常隔离：回调异常不逃出调度线程；重复任务视为返回 false，不再续期。
                reschedule = false;
            }
        }
        lock.lock();
        if (current->repeating && reschedule && !current->cancelled) {
            // 固定延迟语义：以上一次回调结束时刻为基准续期。
            current->expiry = std::chrono::steady_clock::now() + current->period;
            state->timers.insert(std::move(current));
        }
    }
}

}  // namespace

TimerManager::TimerManager()
    : state_(std::make_shared<details::TimerState>())
{
    // 线程自带 state 副本，保证 join 前状态对象始终存活。
    auto state = state_;
    worker_    = std::thread([state]() { run_scheduler_loop(state); });
}

TimerManager::~TimerManager()
{
    auto state = state_;
    if (state != nullptr) {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->shutdown = true;
            state->timers.clear();   // 取消所有未到期任务，析构后不再触发
        }
        state->condition.notify_all();
    }
    if (worker_.joinable())
        worker_.join();   // 等待正在执行的回调返回，期间 state_ 仍然有效
}

TimerHandle TimerManager::schedule_once(std::chrono::milliseconds delay, std::function<void()> callback)
{
    if (!callback)
        return TimerHandle{};
    std::function<bool()> wrapped = [cb = std::move(callback)]() -> bool {
        cb();
        return false;
    };
    return add_timer(delay, std::move(wrapped), false);
}

TimerHandle TimerManager::schedule_repeating(std::chrono::milliseconds period, std::function<bool()> callback)
{
    if (!callback || period <= std::chrono::milliseconds::zero())
        return TimerHandle{};
    return add_timer(period, std::move(callback), true);
}

std::optional<std::chrono::steady_clock::time_point> TimerManager::next_expiry() const
{
    auto state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->timers.empty())
        return std::nullopt;
    return state->timers.begin()->get()->expiry;
}

TimerHandle TimerManager::add_timer(std::chrono::milliseconds delay, std::function<bool()> callback, bool repeating)
{
    auto state = state_;
    auto entry = std::make_shared<details::TimerEntry>();
    entry->repeating = repeating;
    entry->period    = repeating ? delay : std::chrono::milliseconds::zero();
    entry->callback  = std::move(callback);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->shutdown)
            return TimerHandle{};
        entry->id     = state->next_id++;
        entry->expiry = std::chrono::steady_clock::now() +
                        (delay > std::chrono::milliseconds::zero() ? delay : std::chrono::milliseconds::zero());
        state->timers.insert(entry);
        if (state->timers.begin()->get() == entry.get())
            state->condition.notify_all();   // 新任务成为最近到期者，唤醒调度线程重算等待时长
    }
    TimerHandle handle;
    handle.state_ = state;
    handle.entry_ = std::move(entry);
    return handle;
}

TimerHandle::operator bool() const noexcept
{
    return entry_ != nullptr;
}

void TimerHandle::cancel() const noexcept
{
    auto state = state_.lock();
    if (state == nullptr || entry_ == nullptr)
        return;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (entry_->cancelled)
            return;
        entry_->cancelled = true;
        // 仍在队列中的任务立即移除；已出队正在执行的任务此处 erase 不到，
        // 由调度线程在续期前检查 cancelled 阻止下一次触发。
        state->timers.erase(entry_);
    }
    // 锁外 notify，与模块内其它同步点保持一致，避免被唤醒者立刻阻塞在锁上。
    state->condition.notify_all();
}

}  // namespace ca::thread
