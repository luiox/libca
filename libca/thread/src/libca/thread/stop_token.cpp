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
    return state_ != nullptr && state_->requested.load(std::memory_order_acquire);
}

void StopToken::wait() const
{
    if (state_ == nullptr)
        return;

    std::unique_lock<std::mutex> lock(state_->mutex);
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

    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->requested.exchange(true, std::memory_order_acq_rel))
            return false;
    }
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
