#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ca::thread {

/// @brief 借还式对象池：obtain() 借出对象，归还动作由 shared_ptr 自定义 deleter 完成。
///
/// 快路径用 try_lock 复用空闲对象；锁竞争（高并发）、池空或已关闭时退化为直接调用
/// factory 构造新对象，不阻塞调用方。借出对象通过 shared_ptr 归还：最后一个引用释放
/// 时，池存活且未关闭则先执行 on_recycle 重置钩子再回池，否则直接销毁。借出对象可以
/// 比池活得久：deleter 持有 weak_ptr，池析构后归还的对象被直接 delete，不泄漏。
/// ObjectPool 不可复制；同一池可被多线程并发 obtain/归还。
template<typename T>
class ObjectPool
{
public:
    /// @brief 对象工厂，obtain() 无法复用空闲对象时调用，返回新对象。
    using Factory = std::function<std::unique_ptr<T>()>;

    /// @brief 归还钩子，对象回池前调用，用于重置状态。
    /// @note 钩子抛出的异常被吞掉，且该对象会被销毁而不是回池（重置失败的对象状态不可信）。
    using RecycleHook = std::function<void(T&)>;

    /// @brief 创建对象池。
    /// @param factory 对象工厂，不能为空。
    /// @param on_recycle 可选的归还重置钩子。
    /// @throw factory 为空时抛出 std::invalid_argument；状态分配失败时抛出 std::bad_alloc。
    ObjectPool(Factory factory, RecycleHook on_recycle = nullptr)
    {
        if (!factory)
            throw std::invalid_argument("ObjectPool factory must not be empty");
        state_             = std::make_shared<State>();
        state_->factory    = std::move(factory);
        state_->on_recycle = std::move(on_recycle);
    }

    ObjectPool(const ObjectPool&)            = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /// @brief 析构：关闭池并销毁所有空闲对象；已借出对象由其 shared_ptr 最后负责销毁。
    ~ObjectPool()
    {
        shutdown();
    }

    /// @brief 借出一个对象。
    ///
    /// 优先 try_lock 复用空闲对象；锁被占用（高竞争）、池已空或已关闭时直接调用
    /// factory 构造新对象，不阻塞。返回的 shared_ptr 释放最后一个引用时自动归还或销毁。
    /// @throw factory 抛出的异常原样传播。
    std::shared_ptr<T> obtain()
    {
        auto state = state_;
        T* raw     = nullptr;
        {
            std::unique_lock<std::mutex> lock(state->mutex, std::try_to_lock);
            if (lock.owns_lock() && !state->shutdown && !state->idle.empty()) {
                raw = state->idle.back().release();
                state->idle.pop_back();
            }
        }
        if (raw == nullptr)
            raw = state->factory().release();   // 高竞争/池空/已关闭：锁外直接构造
        return std::shared_ptr<T>(raw, Deleter{state_});
    }

    /// @brief 关闭池。之后 obtain() 直接构造新对象，归还（deleter 触发）改为销毁。幂等。
    void shutdown() noexcept
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        state->shutdown = true;
    }

    /// @brief 是否已关闭。
    bool is_shutdown() const noexcept
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->shutdown;
    }

private:
    // 池共享状态：由池本身与各借出对象的 deleter 通过 shared_ptr/weak_ptr 共享，
    // 借出对象可在池析构后安全归还（weak_ptr 失效时直接 delete）。
    struct State
    {
        Factory factory;
        RecycleHook on_recycle;
        mutable std::mutex mutex;
        std::vector<std::unique_ptr<T>> idle;
        bool shutdown{false};
    };

    // 自定义 deleter：最后一个引用释放时归还进池或销毁。
    struct Deleter
    {
        std::weak_ptr<State> state;

        void operator()(T* ptr) const noexcept
        {
            if (ptr == nullptr)
                return;
            auto locked = state.lock();
            if (locked == nullptr) {
                delete ptr;   // 池已析构
                return;
            }
            std::lock_guard<std::mutex> lock(locked->mutex);
            if (locked->shutdown) {
                delete ptr;   // 已关闭：归还改为销毁
                return;
            }
            if (locked->on_recycle) {
                try {
                    locked->on_recycle(*ptr);
                }
                catch (...) {
                    // 重置失败的对象状态不可信：吞掉异常并销毁，不入池。
                    delete ptr;
                    return;
                }
            }
            try {
                locked->idle.emplace_back(ptr);
            }
            catch (...) {
                // 本函数整体 noexcept：回池分配失败（bad_alloc）时销毁对象，而不是 terminate。
                delete ptr;
            }
        }
    };

    std::shared_ptr<State> state_;
};

}  // namespace ca::thread
