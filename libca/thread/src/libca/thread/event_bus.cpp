#include "libca/thread/event_bus.hpp"

#include <exception>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ca::thread {
namespace details {

// 全部字段在 mutex 保护下访问。emit 只拷贝一份监听表快照，回调在锁外执行，
// 因此监听器回调内部可以安全地 subscribe/unsubscribe/emit 其它事件。
class EventBusState
{
public:
    mutable std::mutex mutex;
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, std::function<void()>>> listeners;
    std::uint64_t next_id{1};
};

}  // namespace details

EventBus::EventBus()
    : state_(std::make_shared<details::EventBusState>())
{}

EventBus::~EventBus() = default;

SubscriptionHandle EventBus::subscribe(const std::string& event, Callback callback)
{
    if (!callback)
        return SubscriptionHandle{};
    auto state = state_;
    SubscriptionHandle handle;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        handle.state_ = state;
        handle.event_ = event;
        handle.id_    = state->next_id++;
        state->listeners[event].emplace(handle.id_, std::move(callback));
    }
    return handle;
}

void EventBus::unsubscribe(const SubscriptionHandle& handle) noexcept
{
    handle.unsubscribe();
}

usize EventBus::emit(const std::string& event)
{
    auto state = state_;
    std::vector<std::function<void()>> snapshot;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        auto it = state->listeners.find(event);
        if (it == state->listeners.end())
            return 0;
        snapshot.reserve(it->second.size());
        for (const auto& entry : it->second)
            snapshot.push_back(entry.second);
    }
    usize invoked = 0;
    for (const auto& callback : snapshot) {
        try {
            callback();
        }
        catch (...) {
            // 异常隔离：单个监听器的异常不影响其它监听器，也不逃出 emit。
        }
        ++invoked;
    }
    return invoked;
}

SubscriptionHandle::operator bool() const noexcept
{
    return id_ != 0;
}

void SubscriptionHandle::unsubscribe() const noexcept
{
    auto state = state_.lock();
    if (state == nullptr || id_ == 0)
        return;
    std::lock_guard<std::mutex> lock(state->mutex);
    auto event_it = state->listeners.find(event_);
    if (event_it == state->listeners.end())
        return;
    event_it->second.erase(id_);
    if (event_it->second.empty())
        state->listeners.erase(event_it);   // 空事件及时清理，避免监听表无限增长
}

}  // namespace ca::thread
