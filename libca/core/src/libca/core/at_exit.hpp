#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

/// @file at_exit.hpp
/// @brief 进程级 LIFO 退出回调管理器（chromium 式 AtExitManager）。
/// @note 与 scope_guard 的分工：scope_guard 管理栈作用域内的清理（defer 语义），
///       AtExitManager 管理进程/静态单例生命周期——回调不跟随某个栈帧，而是在
///       manager 析构或显式 Run() 时按注册逆序（LIFO）统一执行，适合静态对象、
///       全局单例、第三方库全局资源的释放。
/// @note 嵌套语义：manager 之间构成全局栈，注册总是发生在"当前"（栈顶）manager 上。
///       子 manager 析构时先执行自己的回调，随后栈顶回落到父级，后续注册重新归父级。

namespace ca::core {

/// @brief 进程/静态单例生命周期的 LIFO 退出回调管理器。
/// @note 典型用法：main() 开头（或全局静态区）构造一个进程级 manager，整个进程
///       共用；作用域内再构造的 manager 形成嵌套（子级），只消费自己任职期间的注册。
///       不可复制、不可移动（manager 被全局栈引用）。
class AtExitManager {
public:
    /// @brief 构造并压入全局 manager 栈，成为新的"当前" manager。
    AtExitManager() noexcept;

    /// @brief 析构：按 LIFO 执行本 manager 收到的全部回调，再从全局栈弹出。
    /// @note noexcept；回调抛出异常将按 C++ 规则直接 std::terminate。
    /// @warning 父子 manager 必须按嵌套顺序析构（子级先于父级）；乱序析构（父级
    ///          先死）会导致后续注册仍流向子级、且子级析构后留下悬空的"当前"
    ///          manager，属未定义用法。
    ~AtExitManager();

    AtExitManager(const AtExitManager&) = delete;
    AtExitManager& operator=(const AtExitManager&) = delete;
    AtExitManager(AtExitManager&&) = delete;
    AtExitManager& operator=(AtExitManager&&) = delete;

    /// @brief 向"当前" manager（全局栈顶）注册退出回调，执行顺序为 LIFO。
    /// @param callback 退出回调；为空的 callback 会被安全跳过。
    /// @return 注册成功返回 true；当前不存在任何 manager 时返回 false。
    /// @note 线程安全。回调在 manager 析构或 Run() 时执行；执行期间（含回调内部）
    ///       新注册的回调同样会被消费，不会丢失。
    static bool register_callback(std::function<void()> callback);

    /// @brief 立即执行"当前" manager（栈顶）已注册的全部回调（LIFO），并清空队列。
    /// @note 线程安全。处理采用"分批取出、锁外执行"：回调执行期间不持锁，回调内部
    ///       可以继续注册，新回调随后被继续消费直到队列排空。Run() 之后 manager
    ///       仍然存活，可继续接收注册（下次析构/Run 时执行）。
    /// @note 当前不存在任何 manager 时为 no-op。
    static void run();

private:
    /// @brief 消费本 manager 的全部回调：分批换出执行，直到队列为空。
    void process_callbacks() noexcept;

    /// @brief 加锁追加一个回调。
    void push(std::function<void()> callback);

    /// @brief 全局 manager 栈顶；nullptr 表示当前没有任何 manager。
    inline static std::atomic<AtExitManager*> g_top_manager_{nullptr};

    AtExitManager* next_manager_{nullptr};
    std::mutex mutex_;
    std::vector<std::function<void()>> callbacks_;
};

inline AtExitManager::AtExitManager() noexcept {
    next_manager_ = g_top_manager_.load(std::memory_order_acquire);
    g_top_manager_.store(this, std::memory_order_release);
}

inline AtExitManager::~AtExitManager() {
    process_callbacks();
    // 仅当自己仍是栈顶时正常弹出；乱序析构属未定义用法（见析构 @warning），不在此修复链表。
    if (g_top_manager_.load(std::memory_order_acquire) == this) {
        g_top_manager_.store(next_manager_, std::memory_order_release);
    }
}

inline bool AtExitManager::register_callback(std::function<void()> callback) {
    AtExitManager* manager = g_top_manager_.load(std::memory_order_acquire);
    if (manager == nullptr) {
        return false;
    }
    manager->push(std::move(callback));
    return true;
}

inline void AtExitManager::run() {
    AtExitManager* manager = g_top_manager_.load(std::memory_order_acquire);
    if (manager != nullptr) {
        manager->process_callbacks();
    }
}

inline void AtExitManager::process_callbacks() noexcept {
    // 分批取出、锁外执行：回调内部可安全注册新回调（进入下一批），并发注册也不阻塞。
    // 回调标记为 noexcept，若用户回调抛异常，按 C++ 规则 std::terminate。
    for (;;) {
        std::vector<std::function<void()>> batch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            batch.swap(callbacks_);
        }
        if (batch.empty()) {
            break;
        }
        for (auto it = batch.rbegin(); it != batch.rend(); ++it) {
            if (*it) {
                std::move(*it)();
            }
        }
    }
}

inline void AtExitManager::push(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

} // namespace ca::core
