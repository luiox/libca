#pragma once

#include "libca/core/datatype.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

/// @file channel.hpp
/// @brief MPSC 通道 —— 多生产者单消费者消息传递，对齐 Rust std::sync::mpsc 的简化版。
///        `channel()` 返回 (Sender, Receiver)；Sender 可拷贝（多生产者），Receiver move-only
///        （单消费者）。capacity=0 表示无界缓冲，capacity>0 表示有界缓冲（满时 send 阻塞）。
///        所有 Sender 销毁后，Receiver 排空已有元素，之后 recv 返回空 optional。
///        命名空间 `ca::thread`。

namespace ca::thread {

// 前向声明：Sender 的 friend 引用了 Receiver 与 channel()，g++ 要求它们在此前已声明。
// 默认实参只能在一份声明上给出，故在此处给出，末尾定义不再重复。
template<typename T>
class Receiver;
template<typename T>
class Sender;
template<typename T>
std::pair<Sender<T>, Receiver<T>> channel(usize capacity = 0);

namespace detail {

template<typename T>
struct ChannelState
{
    explicit ChannelState(usize cap)
        : capacity(cap)
    {}

    /// 是否仍有生产者（含显式关闭）。
    bool producers_alive() const noexcept { return sender_count > 0 && !explicitly_closed; }

    const usize             capacity;
    mutable std::mutex      mutex;
    std::condition_variable not_empty;   // 队列从空变非空时通知消费者
    std::condition_variable not_full;    // 有界队列从满变非满时通知生产者
    std::deque<T>           items;
    usize                   sender_count{0};
    bool                    receiver_alive{true};
    bool                    explicitly_closed{false};   // Sender::close() 提前关闭
};

}   // namespace detail

template<typename T>
class Sender
{
public:
    Sender() noexcept = default;   // 空发送端，send 返回 false。

    // 拷贝 = 新增一个生产者，sender_count++。
    Sender(const Sender& other)
        : state_(other.state_)
    {
        if (state_ != nullptr) {
            std::lock_guard<std::mutex> lock(state_->mutex);
            ++state_->sender_count;
        }
    }

    Sender& operator=(const Sender& other)
    {
        if (this != &other) {
            release();
            state_ = other.state_;
            if (state_ != nullptr) {
                std::lock_guard<std::mutex> lock(state_->mutex);
                ++state_->sender_count;
            }
        }
        return *this;
    }

    Sender(Sender&& other) noexcept = default;
    Sender& operator=(Sender&& other) noexcept
    {
        if (this != &other) {
            release();
            state_ = std::move(other.state_);
        }
        return *this;
    }

    ~Sender() { release(); }

    /// @brief 发送一个值。
    /// @return 成功入队返回 true；通道已关闭（Receiver 销毁或 close）返回 false。
    ///         有界通道满时阻塞等待空位。
    bool send(T value)
    {
        auto state = state_;
        if (state == nullptr)
            return false;

        std::unique_lock<std::mutex> lock(state->mutex);
        if (!state->receiver_alive)
            return false;
        if (state->explicitly_closed)
            return false;
        if (state->capacity > 0) {
            state->not_full.wait(lock, [&] {
                return !state->receiver_alive || state->explicitly_closed ||
                       state->items.size() < state->capacity;
            });
            if (!state->receiver_alive || state->explicitly_closed)
                return false;
        }
        state->items.push_back(std::move(value));
        lock.unlock();
        state->not_empty.notify_one();
        return true;
    }

    /// @brief 显式关闭生产端。所有 Sender 销毁也会自动关闭。
    void close() noexcept
    {
        auto state = state_;
        if (state == nullptr)
            return;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->explicitly_closed = true;
        }
        state->not_empty.notify_all();
        state->not_full.notify_all();
    }

private:
    template<typename U>
    friend class Receiver;

    template<typename U>
    friend std::pair<Sender<U>, Receiver<U>> channel(usize);

    std::shared_ptr<detail::ChannelState<T>> state_;

    explicit Sender(std::shared_ptr<detail::ChannelState<T>> state) noexcept
        : state_(std::move(state))
    {}

    void release() noexcept
    {
        if (state_ == nullptr)
            return;
        bool last = false;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (state_->sender_count > 0)
                --state_->sender_count;
            last = !state_->producers_alive();
        }
        if (last) {
            // 最后一个生产者离开，唤醒所有等待的消费者（recv 应开始返回 nullopt）。
            state_->not_empty.notify_all();
        }
        state_.reset();
    }
};

template<typename T>
class Receiver
{
public:
    Receiver() noexcept                  = default;
    Receiver(const Receiver&)            = delete;
    Receiver& operator=(const Receiver&) = delete;
    Receiver(Receiver&&) noexcept        = default;
    Receiver& operator=(Receiver&& other) noexcept
    {
        if (this != &other) {
            mark_dead();
            state_ = std::move(other.state_);
        }
        return *this;
    }

    ~Receiver() { mark_dead(); }

    /// @brief 阻塞接收一个元素。
    /// @return 有元素返回该值；所有 Sender 销毁/关闭且队列排空后返回空 optional。
    std::optional<T> recv()
    {
        auto state = state_;
        if (state == nullptr)
            return std::nullopt;

        std::unique_lock<std::mutex> lock(state->mutex);
        state->not_empty.wait(lock,
                              [&] { return !state->items.empty() || !state->producers_alive(); });
        if (state->items.empty())
            return std::nullopt;
        T value = std::move(state->items.front());
        state->items.pop_front();
        lock.unlock();
        state->not_full.notify_one();
        return value;
    }

    /// @brief 非阻塞接收。有元素返回值，否则返回空 optional。
    std::optional<T> try_recv()
    {
        auto state = state_;
        if (state == nullptr)
            return std::nullopt;

        T value;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->items.empty())
                return std::nullopt;
            value = std::move(state->items.front());
            state->items.pop_front();
        }
        // 与 recv/recv_timeout 一致：先释放锁再通知，避免被唤醒的生产者立刻阻塞在锁上。
        state->not_full.notify_one();
        return value;
    }

    /// @brief 限时接收。超时或生产端全关闭且排空后返回空 optional。
    std::optional<T> recv_timeout(std::chrono::milliseconds timeout)
    {
        auto state = state_;
        if (state == nullptr)
            return std::nullopt;

        std::unique_lock<std::mutex> lock(state->mutex);
        if (!state->not_empty.wait_for(
                lock, timeout, [&] { return !state->items.empty() || !state->producers_alive(); }))
            return std::nullopt;
        if (state->items.empty())
            return std::nullopt;
        T value = std::move(state->items.front());
        state->items.pop_front();
        lock.unlock();
        state->not_full.notify_one();
        return value;
    }

private:
    template<typename U>
    friend std::pair<Sender<U>, Receiver<U>> channel(usize);

    std::shared_ptr<detail::ChannelState<T>> state_;

    explicit Receiver(std::shared_ptr<detail::ChannelState<T>> state) noexcept
        : state_(std::move(state))
    {}

    void mark_dead() noexcept
    {
        if (state_ == nullptr)
            return;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->receiver_alive = false;
        }
        // 唤醒所有阻塞在 send 的生产者（它们会发现 receiver_alive=false 而 return false）。
        state_->not_full.notify_all();
        state_.reset();
    }
};

/// @brief 创建一个 MPSC 通道。
/// @param capacity 缓冲容量；0 表示无界（仅受内存限制），>0 表示有界（满时 send 阻塞）。
/// @return (Sender, Receiver)。第一个 Sender 的生产者计数已初始化为 1。
template<typename T>
std::pair<Sender<T>, Receiver<T>> channel(usize capacity)
{
    auto state          = std::make_shared<detail::ChannelState<T>>(capacity);
    state->sender_count = 1;
    Sender<T>   sender(state);
    Receiver<T> receiver(state);
    return {std::move(sender), std::move(receiver)};
}

}   // namespace ca::thread
