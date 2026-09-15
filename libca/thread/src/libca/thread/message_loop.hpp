#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "libca/core/status.hpp"
#include "libca/thread/stop_token.hpp"
#include "libca/thread/thread_pool.hpp"

namespace ca::thread {

/// @brief 线程亲和的任务循环：单个工作线程 + FIFO 任务队列。
///
/// MessageLoop 是 event loop 的前身：所有任务都在同一个工作线程上严格按提交顺序
/// 执行，为「单线程持有状态、其它线程投递工作」的异步化模型提供基础。
///
/// 线程安全模型：
/// - 全部公开操作可被多个线程并发调用。
/// - loop 任务内可以继续 post_task / post_delay_task（任务经队列执行，不递归调用栈）。
/// - 任务抛出的异常被吞掉，不会终止工作线程（与 TimerManager 一致）；需要观察异常
///   或取得返回值时使用 post_task_with_result()。
///
/// 关闭语义（两种停止方式的差别只在「未执行任务」的处理上）：
/// - stop()：丢弃队列中所有未执行任务（即时 + 延迟）。
/// - stop_and_drain()：执行完停止时已在队列中的全部即时任务后退出。
/// - 两种停止都不中断正在执行的任务；未到期的延迟任务一律废弃。
/// - 停止后 post 系列返回 FAILED_PRECONDITION 错误。
///
/// @note 析构函数作为兜底执行 stop_and_drain() 和 join()。若任务可能长时间阻塞，
///       调用方应显式选择 stop()/stop_and_drain() 并 join，不要依赖析构兜底。
///       禁止在 loop 任务内析构本对象（会等待自身线程，必然死锁）。
class MessageLoop
{
public:
    MessageLoop(const MessageLoop&)            = delete;
    MessageLoop& operator=(const MessageLoop&) = delete;

    /// @brief 转移 MessageLoop 所有权。
    MessageLoop(MessageLoop&& other) noexcept;

    /// @brief 排空并等待当前 loop 后接管新对象。
    MessageLoop& operator=(MessageLoop&& other) noexcept;

    /// @brief 兜底关闭：执行 stop_and_drain() 和 join()。
    ~MessageLoop();

    /// @brief 创建 MessageLoop 并立即启动工作线程。
    ///
    /// 构造即运行，没有独立 start()；创建后从未投递任务直接析构是安全的。
    /// @return 工作线程创建失败时返回错误。
    static ca::core::StatusResult<MessageLoop> create();

    /// @brief 投递即时任务，严格按提交顺序在 loop 线程执行，本调用立即返回。
    ///
    /// @param function 无参可调用对象；任务抛出的异常被吞掉。
    /// @return 成功返回 OK；loop 已停止返回 FAILED_PRECONDITION；任务对象为空返回
    ///         INVALID_ARGUMENT；任务包装分配失败返回 RESOURCE_EXHAUSTED。
    template<typename Function, typename Callable = std::decay_t<Function>>
    ca::core::Status post_task(Function&& function)
    {
        static_assert(std::is_invocable_v<Callable&>,
                      "MessageLoop task must be invocable without arguments");
        try {
            return enqueue(std::function<void()>(std::forward<Function>(function)));
        }
        catch (const std::bad_alloc& error) {
            return details::task_allocation_error(error);
        }
        catch (const std::exception& error) {
            return details::task_setup_error(error);
        }
        catch (...) {
            return details::task_setup_error();
        }
    }

    /// @brief post_task 的结果形态：function 在 loop 线程执行，返回值或异常保存到
    ///        future，由调用方线程通过 future 取得。
    ///
    /// 取舍说明：这里选择 std::future 形态而非 post_task_and_reply(fn, reply) 回调形态。
    /// reply 形态要求为「任意调用方线程」定义回投目标，而普通线程没有自己的 loop 可以
    /// 接收回调，最终只能退化为调用方阻塞等待；future 形态与 ThreadPool::submit() 同
    /// 风格，由调用方决定何时等待（可用 wait_for 轮询），且天然支持异常传播。
    /// @param function 无参或接受一个 StopToken 的可调用对象（令牌见 stop_token()）。
    /// @return 成功返回 future；loop 已停止、任务包装失败等错误通过 Status 返回。
    template<typename Function, typename Callable = std::decay_t<Function>,
             typename Result = details::TaskResultT<Callable>>
    ca::core::StatusResult<std::future<Result>> post_task_with_result(Function&& function)
    {
        static_assert(std::is_invocable_v<Callable&> || std::is_invocable_v<Callable&, StopToken>,
                      "MessageLoop task must accept no arguments or one StopToken");
        try {
            auto prepared = details::prepare_task(std::forward<Function>(function), stop_token());
            auto status   = enqueue(std::move(prepared.task.run));
            if (status.is_err())
                return ca::core::Err(status);
            return ca::core::Ok(std::move(prepared.future));
        }
        catch (const std::bad_alloc& error) {
            return ca::core::Err(details::task_allocation_error(error));
        }
        catch (const std::exception& error) {
            return ca::core::Err(details::task_setup_error(error));
        }
        catch (...) {
            return ca::core::Err(details::task_setup_error());
        }
    }

    /// @brief 投递延迟任务：不早于 delay 之后在 loop 线程执行。
    ///
    /// 到期后任务转投 loop 队列尾部执行（保证线程亲和，绝不在定时器上下文直接执行），
    /// 因此可能被更早已入队的任务顺延；延迟任务不抢占任何任务。与即时任务共用同一条
    /// FIFO 队列后按到期顺序入队。
    /// @param function 无参可调用对象，异常处理同 post_task()。
    /// @param delay 相对当前的延迟；负值按 0 处理（尽快执行）。
    /// @return 返回值语义同 post_task()。
    template<typename Function, typename Callable = std::decay_t<Function>>
    ca::core::Status post_delay_task(Function&& function, std::chrono::milliseconds delay)
    {
        static_assert(std::is_invocable_v<Callable&>,
                      "MessageLoop task must be invocable without arguments");
        try {
            return enqueue_delay(std::function<void()>(std::forward<Function>(function)), delay);
        }
        catch (const std::bad_alloc& error) {
            return details::task_allocation_error(error);
        }
        catch (const std::exception& error) {
            return details::task_setup_error(error);
        }
        catch (...) {
            return details::task_setup_error();
        }
    }

    /// @brief 请求停止：丢弃队列中所有未执行任务（即时 + 延迟）。
    ///
    /// 正在执行的任务不受影响，会执行完毕。该方法幂等且不等待工作线程结束（可在 loop
    /// 任务内调用，不会死锁）；同时请求共享停止令牌（见 stop_token()）。
    ca::core::Status stop();

    /// @brief 请求停止并排空：执行完停止时已在队列中的全部即时任务后退出。
    ///
    /// 与 stop() 的差别只在未执行任务的处理上：本方法等待已入队任务跑完，但延迟任务
    /// （无论是否到期）一律废弃。停止之后入队的任务被拒绝。该方法幂等且不等待工作线程
    /// 结束（可在 loop 任务内调用，不会死锁）；同时请求共享停止令牌。
    ca::core::Status stop_and_drain();

    /// @brief 等待工作线程结束。
    ///
    /// 必须先调用 stop()/stop_and_drain()，且不能从本 loop 的工作线程内调用。该方法
    /// 幂等，并返回工作线程基础设施的首个错误。
    ca::core::Status join();

    /// @brief 返回运行中任务可观察的共享停止令牌。
    ///
    /// stop()/stop_and_drain() 会请求该令牌停止；长时间运行的任务可自愿观察并提前
    /// 退出，但不影响其它任务，也不会改变关闭语义。
    StopToken stop_token() const noexcept;

    /// @brief 当前调用线程是否为 loop 工作线程；移动后的空对象返回 false。
    bool running_on() const noexcept;

    /// @brief 是否已经请求停止；移动后的空对象返回 true。
    bool is_stopped() const noexcept;

    /// @brief 是否已经完成 join。
    bool is_joined() const noexcept;

    /// @brief 返回尚未开始执行的任务数量（即时 + 延迟）；移动后的空对象返回 0。
    usize pending_task_count() const noexcept;

private:
    class Impl;

    explicit MessageLoop(std::unique_ptr<Impl> impl) noexcept;

    ca::core::Status enqueue(std::function<void()> task);
    ca::core::Status enqueue_delay(std::function<void()> task, std::chrono::milliseconds delay);
    void             finish_noexcept() noexcept;

    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::thread
