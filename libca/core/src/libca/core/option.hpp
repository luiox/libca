/// @file option.hpp
/// @brief Option<T>：可空值语义（对标 Rust Option / Some / None）。
/// @details 与 Result<T, E>（成败语义）互补：表达「可能有值也可能没有」。
///          内部基于 std::optional 存储实现（与 collection 模块包装 STL 容器同一策略），
///          API 面向 Rust 语义：Some/None 工厂、unwrap 家族、组合子与 Result 桥接。
/// @note unwrap()/expect() 在 None 上 std::terminate（与 Result::unwrap 的失败行为一致）；
///       不确定有值时用 unwrap_or / unwrap_or_else。
/// @author Canrad
/// @date 2026/08/25

#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include "libca/core/result.hpp"

namespace ca::core {

/// @brief None 的标签类型：`Option<T> x = None;` 表达空值。
struct NoneT {
    /// @brief 支持比较（哨兵值语义）。
    constexpr bool operator==(const NoneT&) const noexcept { return true; }
    constexpr bool operator!=(const NoneT&) const noexcept { return false; }
};

/// @brief 空值哨兵（对标 Rust 的 None）。
inline constexpr NoneT None{};

/// @brief 可空值（对标 Rust Option<T>）：Some(值) 或 None。
/// @tparam T 值类型；T 须非 void（「可能有 void」退化为 bool，无意义）。
/// @details 拷贝/移动语义跟随 T（T 可拷贝则 Option 可拷贝，T 仅移动则 Option 仅移动）。
template<typename T>
class Option {
    static_assert(!std::is_same<T, void>::value,
                  "Option<void> is not allowed: use bool instead");

public:
    /// @brief 构造 None。
    Option() noexcept = default;
    /// @brief 从 None 哨兵构造（`Option<T> x = None;`）。
    Option(NoneT) noexcept {}
    /// @brief 就地构造值。
    template<typename... Args>
    explicit Option(std::in_place_t, Args&&... args)
        : value_(std::in_place, std::forward<Args>(args)...) {}

    /// @brief 拷贝构造（T 可拷贝时参与重载，语义跟随 std::optional）。
    Option(const Option&) = default;
    /// @brief 移动构造。
    Option(Option&&) = default;
    /// @brief 拷贝赋值。
    Option& operator=(const Option&) = default;
    /// @brief 移动赋值。
    Option& operator=(Option&&) = default;
    /// @brief 赋 None（清空）。
    Option& operator=(NoneT) noexcept {
        value_.reset();
        return *this;
    }

    /// @brief 是否有值。
    bool is_some() const noexcept { return value_.has_value(); }
    /// @brief 是否为空。
    bool is_none() const noexcept { return !value_.has_value(); }
    /// @brief 同 is_some()，支持 `if (opt)` 语境。
    explicit operator bool() const noexcept { return value_.has_value(); }

    // ---- 访问 ----

    /// @brief 取值；None 上打印错误并 std::terminate。确定是 Some 时才用。
    /// @return 存储值的引用。
    T& unwrap() & {
        check_some("Attempting to unwrap a None Option");
        return *value_;
    }
    /// @copydoc unwrap()
    const T& unwrap() const& {
        check_some("Attempting to unwrap a None Option");
        return *value_;
    }
    /// @brief 移动取值（值被移出，Option 保持 Some 但值处于 moved-from 状态）。
    T unwrap() && {
        check_some("Attempting to unwrap a None Option");
        return std::move(*value_);
    }
    /// @brief 取值；None 上打印 `msg` 并 std::terminate。
    T& expect(const char* msg) & {
        check_some(msg);
        return *value_;
    }
    /// @copydoc expect()
    const T& expect(const char* msg) const& {
        check_some(msg);
        return *value_;
    }
    /// @brief 移动取值；None 上打印 `msg` 并 std::terminate。
    T expect(const char* msg) && {
        check_some(msg);
        return std::move(*value_);
    }

    /// @brief 指针式访问（已确认 Some 的快捷路径，等价 unwrap() 的 operator 语法糖）。
    /// @warning None 上解引用是 UB——与 std::optional::operator-> 同契约。
    const T* operator->() const { return &*value_; }
    /// @copydoc operator->()
    T* operator->() { return &*value_; }
    /// @brief 解引用（已确认 Some 的快捷路径）。
    /// @warning None 上解引用是 UB。
    const T& operator*() const& { return *value_; }
    /// @copydoc operator*()
    T& operator*() & { return *value_; }

    // ---- 取值或默认 ----

    /// @brief 有值返回值的拷贝（rvalue 时移动），否则返回 default_value。
    T unwrap_or(T default_value) const& {
        return value_ ? *value_ : std::move(default_value);
    }
    /// @copydoc unwrap_or()
    T unwrap_or(T default_value) && {
        return value_ ? std::move(*value_) : std::move(default_value);
    }
    /// @brief 有值返回值的拷贝/移动，否则调用 default_fn() 取值。
    template<typename Func>
    T unwrap_or_else(Func default_fn) const& {
        return value_ ? *value_ : default_fn();
    }
    /// @copydoc unwrap_or_else()
    template<typename Func>
    T unwrap_or_else(Func default_fn) && {
        return value_ ? std::move(*value_) : default_fn();
    }
    /// @brief 有值返回值的拷贝/移动，否则返回 T{}。
    T unwrap_or_default() const& {
        return value_ ? *value_ : T{};
    }
    /// @copydoc unwrap_or_default()
    T unwrap_or_default() && {
        return value_ ? std::move(*value_) : T{};
    }

    // ---- 组合子 ----

    /// @brief Some 时对值应用 map_fn，返回 Some(结果)；None 透传。
    /// @return Option<U>，U 为 map_fn 的返回类型。
    template<typename Func>
    auto map(Func map_fn) const& -> Option<std::decay_t<decltype(map_fn(std::declval<const T&>()))>> {
        using U = std::decay_t<decltype(map_fn(std::declval<const T&>()))>;
        if (value_) return Option<U>(std::in_place, map_fn(*value_));
        return None;
    }
    /// @copydoc map()
    template<typename Func>
    auto map(Func map_fn) && -> Option<std::decay_t<decltype(map_fn(std::declval<T&&>()))>> {
        using U = std::decay_t<decltype(map_fn(std::declval<T&&>()))>;
        if (value_) return Option<U>(std::in_place, map_fn(std::move(*value_)));
        return None;
    }
    /// @brief Some 时调用 and_fn(值)，其返回的 Option<U> 直接作为结果；None 透传。
    /// @return Option<U>，U 由 and_fn 的返回类型推导。
    template<typename Func>
    auto and_then(Func and_fn) const& -> decltype(and_fn(std::declval<const T&>())) {
        using Ret = decltype(and_fn(std::declval<const T&>()));
        static_assert(std::is_same<Ret, Option<typename Ret::value_type>>::value,
                      "and_then expects a function returning an Option");
        if (value_) return and_fn(*value_);
        return None;
    }
    /// @copydoc and_then()
    template<typename Func>
    auto and_then(Func and_fn) && -> decltype(and_fn(std::declval<T&&>())) {
        using Ret = decltype(and_fn(std::declval<T&&>()));
        static_assert(std::is_same<Ret, Option<typename Ret::value_type>>::value,
                      "and_then expects a function returning an Option");
        if (value_) return and_fn(std::move(*value_));
        return None;
    }
    /// @brief None 时调用 or_fn() 取替代 Option；Some 原样返回。
    template<typename Func>
    Option<T> or_else(Func or_fn) const& {
        if (value_) return *this;
        return or_fn();
    }
    /// @copydoc or_else()
    template<typename Func>
    Option<T> or_else(Func or_fn) && {
        if (value_) return std::move(*this);
        return or_fn();
    }
    /// @brief 移出值并返回 Some(值)，自身变为 None（对标 Rust Option::take）。
    Option<T> take() noexcept {
        if (!value_) return None;
        Option<T> out(std::in_place, std::move(*value_));
        value_.reset();
        return out;
    }

    // ---- Result 桥接 ----

    /// @brief 转成 Result：Some → Ok(值)，None → Err(err_value)。
    template<typename E>
    Result<T, E> ok_or(E err_value) const& {
        if (value_) return Ok(*value_);
        return Err(std::move(err_value));
    }
    /// @copydoc ok_or()
    template<typename E>
    Result<T, E> ok_or(E err_value) && {
        if (value_) return Ok(std::move(*value_));
        return Err(std::move(err_value));
    }

    // ---- 比较 / 交换 ----

    /// @brief 值相等：同为 None，或同为 Some 且值相等。
    bool operator==(const Option& other) const {
        return value_ == other.value_;
    }
    /// @brief 不等（取反 operator==）。
    bool operator!=(const Option& other) const {
        return value_ != other.value_;
    }
    /// @brief 与 None 哨兵比较（`opt == None`）。
    bool operator==(NoneT) const noexcept { return is_none(); }
    /// @brief 与 None 哨兵不等比较。
    bool operator!=(NoneT) const noexcept { return is_some(); }
    /// @brief 与另一 Option 交换内容。
    void swap(Option& other) noexcept { value_.swap(other.value_); }

    /// @brief 值类型别名（组合子约束用）。
    using value_type = T;

private:
    void check_some(const char* msg) const {
        if (!value_) {
            std::fprintf(stderr, "%s\n", msg);
            std::terminate();
        }
    }

    std::optional<T> value_;
};

/// @brief 构造 Some(值)（对标 Rust Some）。按值衰减推导 Option 的 T。
/// @example `Option<int> x = Some(42);`
template<typename T>
Option<std::decay_t<T>> Some(T&& value) {
    return Option<std::decay_t<T>>(std::in_place, std::forward<T>(value));
}

/// @brief 两个 Option 交换内容。
template<typename T>
void swap(Option<T>& lhs, Option<T>& rhs) noexcept {
    lhs.swap(rhs);
}

// ============================================================================
// Result::ok() / err() 桥接的离线定义
// （声明在 result.hpp；定义放这里以避免 result.hpp → option.hpp 循环依赖）
// ============================================================================

template<typename T, typename E>
template<typename U>
typename std::enable_if<!std::is_same<U, void>::value, Option<U>>::type
Result<T, E>::ok() const& {
    return is_ok() ? Some(storage().template get<T>()) : None;
}

template<typename T, typename E>
template<typename U>
typename std::enable_if<!std::is_same<U, void>::value, Option<U>>::type
Result<T, E>::ok() && {
    return is_ok() ? Some(std::move(storage().template get<T>())) : None;
}

template<typename T, typename E>
Option<E> Result<T, E>::err() const& {
    return is_err() ? Some(storage().template get<E>()) : None;
}

template<typename T, typename E>
Option<E> Result<T, E>::err() && {
    return is_err() ? Some(std::move(storage().template get<E>())) : None;
}

}  // namespace ca::core

namespace ca {
// 对齐 Result 的导出方式：Some/None/Option 一并进 ca 顶层命名空间。
using core::None;
using core::NoneT;
using core::Some;
template<typename T>
using Option = core::Option<T>;
}  // namespace ca

/// @brief std::hash 特化：None 与 Some(v) 的哈希区分，值哈希跟随 std::hash<T>。
template<typename T>
struct std::hash<ca::core::Option<T>> {
    size_t operator()(const ca::core::Option<T>& opt) const noexcept {
        return opt.is_some() ? std::hash<T>{}(opt.unwrap()) : 0;
    }
};
