#pragma once

#include <type_traits>

namespace ca::core {

template <typename T>
struct TypeOf;

template <typename T>
struct TypeOf<const T> : TypeOf<T> {};

namespace detail {
    template <typename T> struct remove_star { using type = T; };
    template <typename T> struct remove_star<T*> { using type = T; };

    template <typename T>
    using cleanup_t = std::remove_cv_t<typename remove_star<T>::type>;
}

template <typename T, typename U>
auto cast(U* ptr) {
    using RawT = detail::cleanup_t<T>;
    return static_cast<RawT*>(ptr);
}

template <typename T, typename U>
auto cast(const U* ptr) {
    using RawT = detail::cleanup_t<T>;
    return static_cast<const RawT*>(ptr);
}

template <typename T, typename U>
bool isa(const U* ptr) {
    using RawT = detail::cleanup_t<T>;
    return ptr && ptr->getType() == TypeOf<RawT>::value;
}

template <typename T, typename U>
auto dyn_cast(const U* ptr) -> decltype(cast<T>(ptr)) {
    return isa<T>(ptr) ? cast<T>(ptr) : nullptr;
}

} // namespace ca::core

namespace typed = ca::core;
