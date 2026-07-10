#pragma once

#include <chrono>
#include <memory>

namespace ca::thread {

namespace details {
class StopState;
}

/// @brief 可复制的协作停止观察句柄。
///
/// StopToken 与创建它的 StopSource 共享停止状态。停止请求是单向且幂等的；令牌只负责
/// 查询或等待请求，不会强制终止线程，也不会中断任意系统调用。
class StopToken
{
public:
    /// @brief 构造一个不关联停止状态的令牌。
    StopToken() noexcept = default;

    /// @brief 当前令牌是否关联了可请求的停止状态。
    bool stop_possible() const noexcept;

    /// @brief 是否已经收到停止请求。
    bool stop_requested() const noexcept;

    /// @brief 阻塞到收到停止请求；无停止状态时立即返回。
    void wait() const;

    /// @brief 最多等待指定时长。
    /// @return 收到停止请求返回 true，超时或无停止状态返回 false。
    bool wait_for(std::chrono::milliseconds timeout) const;

private:
    explicit StopToken(std::shared_ptr<details::StopState> state) noexcept;

    std::shared_ptr<details::StopState> state_;

    friend class StopSource;
};

/// @brief 可复制的协作停止请求源。
///
/// StopSource 默认创建一个新的共享停止状态。复制 StopSource 后，任意副本发出的停止请求
/// 都会被同一状态上的全部 StopToken 观察到。
class StopSource
{
public:
    /// @brief 创建一个尚未请求停止的新状态。
    StopSource();

    /// @brief 返回观察同一停止状态的令牌。
    StopToken token() const noexcept;

    /// @brief 请求停止。
    /// @return 本次调用完成首次状态转换时返回 true，已经请求过时返回 false。
    bool request_stop() noexcept;

    /// @brief 当前停止源是否仍关联共享状态。
    bool stop_possible() const noexcept;

    /// @brief 是否已经请求停止。
    bool stop_requested() const noexcept;

private:
    std::shared_ptr<details::StopState> state_;
};

}   // namespace ca::thread
