#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "libca/core/status.hpp"

namespace ca::thread {

/// @brief 多生产者、多消费者的 move-only 有界阻塞队列。
///
/// BoundedQueue 通过固定容量提供背压。close() 禁止新元素进入但允许消费者排空已有
/// 元素；关闭且排空后，pop 系列返回 CANCELLED。公开操作可被多个线程并发调用。
template<typename T>
class BoundedQueue
{
public:
    BoundedQueue(const BoundedQueue&)            = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) noexcept = default;
    BoundedQueue& operator=(BoundedQueue&&) noexcept = default;

    /// @brief 创建指定容量的队列。
    /// @return 容量为 0 时返回 INVALID_ARGUMENT。
    static ca::core::StatusResult<BoundedQueue> create(usize capacity)
    {
        if (capacity == 0)
            return ca::core::Err(ca::core::ErrStatus(ca::core::StatusCode::INVALID_ARGUMENT,
                                                      "queue capacity must be nonzero"));
        try {
            return ca::core::Ok(BoundedQueue(std::make_shared<State>(capacity)));
        }
        catch (const std::exception& error) {
            return ca::core::Err(ca::core::ErrStatus(
                ca::core::StatusCode::RESOURCE_EXHAUSTED,
                std::string("queue allocation failed: ") + error.what()));
        }
    }

    /// @brief 在队列满时等待，直到元素入队或队列关闭。
    ca::core::Status push(T value)
    {
        auto state = state_;
        if (state == nullptr)
            return invalid_state("push");

        std::unique_lock<std::mutex> lock(state->mutex);
        state->not_full.wait(lock, [&]() {
            return state->closed || state->items.size() < state->capacity;
        });
        if (state->closed)
            return closed_for_push();
        state->items.push_back(std::move(value));
        lock.unlock();
        state->not_empty.notify_one();
        return ca::core::OkStatus();
    }

    /// @brief 尝试立即入队。
    /// @return 入队成功为 true，队列已满为 false，队列关闭则返回错误。
    ca::core::StatusResult<bool> try_push(T value)
    {
        auto state = state_;
        if (state == nullptr)
            return ca::core::Err(invalid_state("try_push"));

        std::unique_lock<std::mutex> lock(state->mutex);
        if (state->closed)
            return ca::core::Err(closed_for_push());
        if (state->items.size() >= state->capacity)
            return ca::core::Ok(false);
        state->items.push_back(std::move(value));
        lock.unlock();
        state->not_empty.notify_one();
        return ca::core::Ok(true);
    }

    /// @brief 在指定时长内等待容量。
    /// @return 入队成功为 true，超时为 false，队列关闭则返回错误。
    ca::core::StatusResult<bool> push_for(T value, std::chrono::milliseconds timeout)
    {
        auto state = state_;
        if (state == nullptr)
            return ca::core::Err(invalid_state("push_for"));

        const auto duration = std::max(timeout, std::chrono::milliseconds::zero());
        std::unique_lock<std::mutex> lock(state->mutex);
        if (!state->not_full.wait_for(lock, duration, [&]() {
                return state->closed || state->items.size() < state->capacity;
            }))
            return ca::core::Ok(false);
        if (state->closed)
            return ca::core::Err(closed_for_push());
        state->items.push_back(std::move(value));
        lock.unlock();
        state->not_empty.notify_one();
        return ca::core::Ok(true);
    }

    /// @brief 在队列为空时等待，直到取得元素或队列关闭并排空。
    ca::core::StatusResult<T> pop()
    {
        auto state = state_;
        if (state == nullptr)
            return ca::core::Err(invalid_state("pop"));

        std::unique_lock<std::mutex> lock(state->mutex);
        state->not_empty.wait(lock, [&]() { return state->closed || !state->items.empty(); });
        if (state->items.empty())
            return ca::core::Err(closed_for_pop());
        T value = std::move(state->items.front());
        state->items.pop_front();
        lock.unlock();
        state->not_full.notify_one();
        return ca::core::Ok(std::move(value));
    }

    /// @brief 尝试立即出队。
    /// @return 有元素时返回该元素，开放但为空时返回空 optional，关闭且为空时返回错误。
    ca::core::StatusResult<std::optional<T>> try_pop()
    {
        auto state = state_;
        if (state == nullptr)
            return ca::core::Err(invalid_state("try_pop"));

        std::unique_lock<std::mutex> lock(state->mutex);
        if (state->items.empty()) {
            if (state->closed)
                return ca::core::Err(closed_for_pop());
            return ca::core::Ok(std::optional<T>{});
        }
        T value = std::move(state->items.front());
        state->items.pop_front();
        lock.unlock();
        state->not_full.notify_one();
        return ca::core::Ok(std::optional<T>(std::move(value)));
    }

    /// @brief 在指定时长内等待元素。
    /// @return 有元素时返回该元素，超时返回空 optional，关闭且为空时返回错误。
    ca::core::StatusResult<std::optional<T>> pop_for(std::chrono::milliseconds timeout)
    {
        auto state = state_;
        if (state == nullptr)
            return ca::core::Err(invalid_state("pop_for"));

        const auto duration = std::max(timeout, std::chrono::milliseconds::zero());
        std::unique_lock<std::mutex> lock(state->mutex);
        if (!state->not_empty.wait_for(
                lock, duration, [&]() { return state->closed || !state->items.empty(); }))
            return ca::core::Ok(std::optional<T>{});
        if (state->items.empty())
            return ca::core::Err(closed_for_pop());
        T value = std::move(state->items.front());
        state->items.pop_front();
        lock.unlock();
        state->not_full.notify_one();
        return ca::core::Ok(std::optional<T>(std::move(value)));
    }

    /// @brief 关闭生产端并允许消费者排空已有元素。
    /// @return 本次调用首次关闭队列时返回 true。
    bool close() noexcept
    {
        auto state = state_;
        if (state == nullptr)
            return false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->closed)
                return false;
            state->closed = true;
        }
        state->not_empty.notify_all();
        state->not_full.notify_all();
        return true;
    }

    /// @brief 关闭队列并取出全部尚未消费的元素。
    ///
    /// 该操作用于取消待处理工作。返回后队列为空，全部等待者都会被唤醒。
    std::deque<T> close_and_take()
    {
        auto state = state_;
        if (state == nullptr)
            return {};

        std::deque<T> pending;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->closed = true;
            pending.swap(state->items);
        }
        state->not_empty.notify_all();
        state->not_full.notify_all();
        return pending;
    }

    /// @brief 返回固定容量；移动后的空对象返回 0。
    usize capacity() const noexcept
    {
        return state_ == nullptr ? 0 : state_->capacity;
    }

    /// @brief 返回当前元素数量。
    usize size() const noexcept
    {
        auto state = state_;
        if (state == nullptr)
            return 0;
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->items.size();
    }

    /// @brief 队列是否已经关闭生产端。
    bool is_closed() const noexcept
    {
        auto state = state_;
        if (state == nullptr)
            return true;
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->closed;
    }

private:
    struct State
    {
        explicit State(usize value)
            : capacity(value)
        {}

        const usize             capacity;
        mutable std::mutex      mutex;
        std::condition_variable not_empty;
        std::condition_variable not_full;
        std::deque<T>           items;
        bool                    closed{false};
    };

    explicit BoundedQueue(std::shared_ptr<State> state) noexcept
        : state_(std::move(state))
    {}

    static ca::core::Status invalid_state(const char* operation)
    {
        return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                   std::string(operation) + " on a moved BoundedQueue");
    }

    static ca::core::Status closed_for_push()
    {
        return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                   "queue is closed for producers");
    }

    static ca::core::Status closed_for_pop()
    {
        return ca::core::ErrStatus(ca::core::StatusCode::CANCELLED,
                                   "queue is closed and empty");
    }

    std::shared_ptr<State> state_;
};

}   // namespace ca::thread
