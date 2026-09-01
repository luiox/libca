#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

#include "libca/core/status.hpp"
#include "libca/thread/stop_token.hpp"

namespace ca::thread {

namespace details {

class ThreadCompletion;

std::shared_ptr<ThreadCompletion> make_thread_completion();
void record_thread_exception(const std::shared_ptr<ThreadCompletion>& completion,
                             std::exception_ptr                       exception) noexcept;

template<typename Function>
void invoke_thread_function(Function& function, const StopToken& token)
{
    if constexpr (std::is_invocable_v<Function&, StopToken>) {
        std::invoke(function, token);
    }
    else {
        static_assert(std::is_invocable_v<Function&>,
                      "Thread callable must accept no arguments or one StopToken");
        std::invoke(function);
    }
}

}   // namespace details

/// @brief 具有协作停止和作用域 join 语义的结构化线程。
///
/// Thread 是 move-only 类型。析构函数会请求停止并等待线程结束，避免 joinable 的
/// std::thread 触发 std::terminate。线程入口抛出的异常被隔离，并由 join() 转换为
/// Status 返回。该类型不提供 detach()。
class Thread
{
public:
    /// @brief 构造一个不拥有线程的空对象。
    Thread() noexcept = default;

    /// @brief 请求停止并等待仍在运行的线程。
    ~Thread();

    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief 转移线程所有权。
    Thread(Thread&& other) noexcept;

    /// @brief 在接管新线程前停止并等待当前拥有的线程。
    Thread& operator=(Thread&& other) noexcept;

    /// @brief 启动一个结构化线程。
    ///
    /// callable 可以不接收参数，也可以接收一个 StopToken。线程创建失败通过 StatusResult
    /// 返回；callable 在线程入口抛出的异常由 join() 报告。
    template<typename Function>
    static ca::core::StatusResult<Thread> start(Function&& function)
    {
        using Callable = std::decay_t<Function>;
        static_assert(std::is_invocable_v<Callable&> || std::is_invocable_v<Callable&, StopToken>,
                      "Thread callable must accept no arguments or one StopToken");

        try {
            StopSource  source;
            StopToken   token      = source.token();
            auto        completion = details::make_thread_completion();
            std::thread native([callable = Callable(std::forward<Function>(function)),
                                token,
                                completion]() mutable noexcept {
                try {
                    details::invoke_thread_function(callable, token);
                }
                catch (...) {
                    details::record_thread_exception(completion, std::current_exception());
                }
            });
            return ca::core::Ok(
                Thread(std::move(native), std::move(source), std::move(completion)));
        }
        catch (const std::system_error& error) {
            return ca::core::Err(
                ca::core::ErrStatus(ca::core::StatusCode::RESOURCE_EXHAUSTED,
                                    std::string("thread creation failed: ") + error.what()));
        }
        catch (const std::exception& error) {
            return ca::core::Err(
                ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                                    std::string("thread setup failed: ") + error.what()));
        }
        catch (...) {
            return ca::core::Err(
                ca::core::ErrStatus(ca::core::StatusCode::INTERNAL, "thread setup failed"));
        }
    }

    /// @brief 当前对象是否拥有可 join 的原生线程。
    bool joinable() const noexcept;

    /// @brief 返回原生线程标识；空对象返回默认构造的 id。
    std::thread::id id() const noexcept;

    /// @brief 返回线程入口观察的停止令牌。
    StopToken stop_token() const noexcept;

    /// @brief 请求线程协作停止，不等待线程结束。
    bool request_stop() noexcept;

    /// @brief 等待线程结束并返回缓存的完成状态。
    ///
    /// 线程入口异常转换为 INTERNAL。对已经成功 join 的 Thread 重复调用会返回同一状态。
    ca::core::Status join();

private:
    Thread(std::thread native, StopSource stop_source,
           std::shared_ptr<details::ThreadCompletion> completion) noexcept;

    void finish_noexcept() noexcept;

    std::thread                                native_;
    std::optional<StopSource>                  stop_source_;
    std::shared_ptr<details::ThreadCompletion> completion_;
    bool                                       started_{false};
    bool                                       joined_{false};
    ca::core::Status                           join_status_;
};

}   // namespace ca::thread
