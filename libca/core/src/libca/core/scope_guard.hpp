#pragma once

#include <type_traits>
#include <utility>

/// @file scope_guard.hpp
/// @brief 作用域退出回调工具。用于在 C++17 中表达 defer/cleanup 语义。

namespace ca::core {

/// @brief 在对象析构时执行一次回调的 RAII 守卫。
/// @tparam F 可调用对象类型。
/// @note 析构函数为 noexcept；如果回调抛异常，程序会按 C++ noexcept 规则终止。
template<typename F>
class ScopeGuard {
public:
    /// @brief 构造一个处于激活状态的作用域守卫。
    explicit ScopeGuard(F func) noexcept(std::is_nothrow_move_constructible<F>::value)
        : active_(true), func_(std::move(func)) {}

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    /// @brief 移动构造，转移回调执行权。
    ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible<F>::value)
        : active_(std::exchange(other.active_, false)), func_(std::move(other.func_)) {}

    ScopeGuard& operator=(ScopeGuard&&) = delete;

    /// @brief 析构时若仍处于激活状态，则执行回调。
    ~ScopeGuard() noexcept {
        if (active_) {
            func_();
        }
    }

    /// @brief 取消析构时执行回调。
    void dismiss() noexcept {
        active_ = false;
    }

    /// @brief 当前是否仍会在析构时执行回调。
    bool is_active() const noexcept {
        return active_;
    }

private:
    bool active_;
    F func_;
};

/// @brief 根据可调用对象推导类型并构造 ScopeGuard。
template<typename F>
ScopeGuard<std::decay_t<F>> make_scope_guard(F&& func) {
    return ScopeGuard<std::decay_t<F>>(std::forward<F>(func));
}

} // namespace ca::core

namespace ca {
    using core::ScopeGuard;
    using core::make_scope_guard;
}

#define LIBCA_DEFER_CONCAT_IMPL(a, b) a##b
#define LIBCA_DEFER_CONCAT(a, b) LIBCA_DEFER_CONCAT_IMPL(a, b)

/// @brief 在当前作用域退出时执行代码块。
/// @note 示例：`DEFER(cleanup());`。回调以引用捕获当前作用域变量。
#define DEFER(...) \
    auto LIBCA_DEFER_CONCAT(_libca_defer_, __LINE__) = ::ca::core::make_scope_guard([&]() { __VA_ARGS__; })
