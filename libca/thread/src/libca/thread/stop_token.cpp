#include "libca/thread/stop_token.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace ca::thread {
namespace details {

class StopState
{
public:
    std::atomic<bool>       requested{false};
    std::mutex              mutex;
    std::condition_variable condition;
};

}   // namespace details

StopToken::StopToken(std::shared_ptr<details::StopState> state) noexcept
    : state_(std::move(state))
{}

bool StopToken::stop_possible() const noexcept
{
    return state_ != nullptr;
}

bool StopToken::stop_requested() const noexcept
{
    // acquire：与 request_stop() 中 exchange 的 acq_rel 配对，确保读到 true 时
    // 该线程也能看到 request_stop 调用方在 stop 之前对共享数据的所有写入。
    return state_ != nullptr && state_->requested.load(std::memory_order_acquire);
}

void StopToken::wait() const
{
    if (state_ == nullptr)
        return;

    std::unique_lock<std::mutex> lock(state_->mutex);
    // 谓词内用 acquire 重读 requested：与 request_stop() 的 acq_rel 配对，
    // 防止虚假唤醒或漏醒（notify 在锁外，见 request_stop() 注释）。
    state_->condition.wait(
        lock, [&]() { return state_->requested.load(std::memory_order_acquire); });
}

bool StopToken::wait_for(std::chrono::milliseconds timeout) const
{
    if (state_ == nullptr)
        return false;

    const auto duration = std::max(timeout, std::chrono::milliseconds::zero());
    std::unique_lock<std::mutex> lock(state_->mutex);
    return state_->condition.wait_for(
        lock, duration, [&]() { return state_->requested.load(std::memory_order_acquire); });
}

StopSource::StopSource()
    : state_(std::make_shared<details::StopState>())
{}

StopToken StopSource::token() const noexcept
{
    return StopToken(state_);
}

bool StopSource::request_stop() noexcept
{
    if (state_ == nullptr)
        return false;

    // 锁内做 exchange：确保多个并发 request_stop() 串行，只有真正把 false 翻成
    // true 的那一次返回 true（即首次请求者）。acq_rel 让两侧的普通读写不重排。
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->requested.exchange(true, std::memory_order_acq_rel))
            return false;
    }
    // notify_all 必须在锁外：持锁唤醒会让被唤醒线程立即醒来又阻塞在 mutex 上，
    // 反而增加额外上下文切换。wait() 的谓词用 acquire 重读 requested，因此即使
    // notify 与此处解锁 race，最坏只是 wait 端多走一次谓词检查，不会漏醒。
    state_->condition.notify_all();
    return true;
}

bool StopSource::stop_possible() const noexcept
{
    return state_ != nullptr;
}

bool StopSource::stop_requested() const noexcept
{
    return state_ != nullptr && state_->requested.load(std::memory_order_acquire);
}

}   // namespace ca::thread
