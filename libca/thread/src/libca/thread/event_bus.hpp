#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "libca/core/datatype.hpp"

namespace ca::thread {

namespace details {

class EventBusState;

}  // namespace details

/// @brief 事件订阅句柄，用于注销订阅。
///
/// 句柄可复制。unsubscribe() 线程安全、幂等且不抛异常；EventBus 销毁后调用退化为
/// 安全的空操作。
class SubscriptionHandle
{
public:
    /// @brief 构造无效句柄，unsubscribe() 为空操作。
    SubscriptionHandle() noexcept = default;

    /// @brief 是否为 subscribe() 返回的有效句柄。
    explicit operator bool() const noexcept;

    /// @brief 注销对应订阅。
    ///
    /// 线程安全且幂等。若某次 emit 正在执行，本轮拷贝的监听表仍会调用刚注销的监听器，
    /// 之后的 emit 不再调用。允许在监听器回调内部注销自身或其它订阅。
    void unsubscribe() const noexcept;

private:
    friend class EventBus;

    std::weak_ptr<details::EventBusState> state_;
    std::string   event_;
    std::uint64_t id_{0};
};

/// @brief 进程内按字符串事件名发布/订阅的事件总线。
///
/// subscribe()/unsubscribe()/emit() 均线程安全。emit() 在锁内拷贝监听表快照、锁外逐个
/// 同步调用，回调执行期间不持有总线内部锁。监听器回调内禁止同步 emit 同一事件（会
/// 无限递归），emit 其它事件是安全的。单个监听器抛出的异常被捕获并忽略，其余监听器
/// 照常调用。事件名不携带负载，需要共享数据时由回调 lambda 自行捕获。
class EventBus
{
public:
    /// @brief 事件回调签名。
    using Callback = std::function<void()>;

    /// @brief 创建空的事件总线。
    EventBus();

    /// @brief 析构并丢弃全部订阅关系；不等待进行中的 emit。
    ~EventBus();

    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;

    /// @brief 订阅事件。
    /// @param event 事件名，区分大小写的精确匹配。
    /// @param callback 回调，不能为空。
    /// @return 订阅句柄；callback 为空时返回无效句柄。同一回调可重复订阅，
    ///         各自获得独立句柄并都会被调用。监听器调用顺序不保证。
    SubscriptionHandle subscribe(const std::string& event, Callback callback);

    /// @brief 注销订阅，等价于 handle.unsubscribe()。幂等，无效句柄为空操作。
    void unsubscribe(const SubscriptionHandle& handle) noexcept;

    /// @brief 发布事件：拷贝当前监听表快照后锁外逐个同步调用。
    ///
    /// 使用调用开始时的监听表快照：过程中注销的监听器本轮仍可能被调用，新订阅者从
    /// 下一次 emit 生效。
    /// @return 本次被调用的监听器数量（抛异常的监听器也计入）。
    usize emit(const std::string& event);

private:
    std::shared_ptr<details::EventBusState> state_;
};

}  // namespace ca::thread
