#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

/// @file once.hpp
/// @brief OnceCell / OnceLock —— 延迟一次性初始化，对齐 Rust std::cell::OnceCell
///        与 std::sync::OnceLock。OnceCell 非线程安全、OnceLock 线程安全。
///        命名空间 `ca::sync`。

namespace ca::sync {

/// @brief 非线程安全的延迟初始化容器。
///
/// 内部存储至多一个 `T`，`get_or_init` 只在首次调用时执行 factory，之后返回缓存值。
/// 适合单线程内的懒初始化；跨线程使用请用 OnceLock。
template<typename T>
class OnceCell
{
public:
    OnceCell() noexcept = default;
    OnceCell(const OnceCell&)            = delete;
    OnceCell& operator=(const OnceCell&) = delete;
    OnceCell(OnceCell&&) noexcept        = default;
    OnceCell& operator=(OnceCell&&) noexcept = default;

    /// @brief 返回已初始化值的指针，未初始化返回 nullptr。
    T* get() noexcept
    {
        return value_.has_value() ? &(*value_) : nullptr;
    }

    /// @brief 返回已初始化值的只读指针，未初始化返回 nullptr。
    const T* get() const noexcept
    {
        return value_.has_value() ? &(*value_) : nullptr;
    }

    /// @brief 是否已完成初始化。
    bool is_initialized() const noexcept
    {
        return value_.has_value();
    }

    /// @brief 首次调用执行 factory 并缓存；后续调用直接返回缓存引用。
    /// @return 指向缓存值的引用（本次调用是否执行了 factory 由 had_value 区分）。
    template<typename Factory>
    T& get_or_init(Factory&& factory)
    {
        static_assert(std::is_invocable_r_v<T, Factory>,
                      "factory must return a value convertible to T");
        if (!value_.has_value())
            value_.emplace(std::forward<Factory>(factory)());
        return *value_;
    }

    /// @brief 取出已缓存的值；未初始化返回空 optional。取出后回到未初始化状态。
    std::optional<T> take() noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        std::optional<T> result;
        if (value_.has_value())
            result = std::move(*value_);
        value_.reset();
        return result;
    }

private:
    std::optional<T> value_;
};

/// @brief 线程安全的延迟初始化容器。
///
/// 多线程并发 `get_or_init` 时只有一个线程执行 factory，其余线程阻塞等待结果。
/// factory 必须是可重复调用安全的（实现不保证取消未中选的 factory 调用，但保证
/// 最终只缓存一个值）。可用于全局单例，规避 static initialization order fiasco。
template<typename T>
class OnceLock
{
public:
    OnceLock() noexcept = default;
    OnceLock(const OnceLock&)            = delete;
    OnceLock& operator=(const OnceLock&) = delete;
    OnceLock(OnceLock&&)                 = delete;
    OnceLock& operator=(OnceLock&&)      = delete;

    /// @brief 返回已初始化值的指针，未初始化返回 nullptr。
    T* get() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_.has_value() ? &(*value_) : nullptr;
    }

    /// @brief 返回已初始化值的只读指针，未初始化返回 nullptr。
    const T* get() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_.has_value() ? &(*value_) : nullptr;
    }

    /// @brief 是否已完成初始化。
    bool is_initialized() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_.has_value();
    }

    /// @brief 首次调用执行 factory 并缓存；后续调用直接返回缓存引用。
    ///
    /// 多线程并发调用时，内部用锁保证 factory 只在未初始化分支被执行一次；factory
    /// 执行期间持有锁，因此 factory 内不应反向回调本对象，避免死锁。
    template<typename Factory>
    T& get_or_init(Factory&& factory)
    {
        static_assert(std::is_invocable_r_v<T, Factory>,
                      "factory must return a value convertible to T");
        std::lock_guard<std::mutex> lock(mutex_);
        if (!value_.has_value())
            value_.emplace(std::forward<Factory>(factory)());
        return *value_;
    }

private:
    mutable std::mutex mutex_;
    std::optional<T>   value_;
};

}  // namespace ca::sync
