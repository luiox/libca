#pragma once

#include <cstdio>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

/// @file result.hpp
/// @brief Result<T, E> —— 用返回值替代异常的错误处理类型，对齐 Rust std::result。
///        成功用 Ok(value)、失败用 Err(error)，命名空间 ca::core（兼容导出到 ca）。
/// @note 约束：E 不能是 void；T 可以是 void。取值失败（unwrap 错误值等）会 std::terminate。
///       链式 API：map/map_error/then/otherwise/and_then/or_else。
///       TRY 宏依赖 GNU statement expression，仅 GCC/Clang 可用（见宏处说明）。
/// @attention 内部 details/ok/err/And/Or/Other 等命名空间是实现机制，非公开接口。

namespace ca::core {

/*
   Mathieu Stefani, 03 mai 2016

   This header provides a Result type that can be used to replace exceptions in code
   that has to handle error.

   Result<T, E> can be used to return and propagate an error to the caller. Result<T, E> is an algebraic
   data type that can either Ok(T) to represent success or Err(E) to represent an error.
*/

/// 成功/错误值的包装类型，通常不直接用，经 Ok()/Err() 工厂构造。
namespace types {
    template<typename T>
    struct Ok {
        Ok(const T& val) : val(val) { }
        Ok(T&& val) : val(std::move(val)) { }

        T val;
    };

    template<>
    struct Ok<void> { };

    template<typename E>
    struct Err {
        Err(const E& val) : val(val) { }
        Err(E&& val) : val(std::move(val)) { }

        E val;
    };
}

/// @brief 构造成功值：`return Ok(42);`。值类型自动 decay。
template<typename T, typename CleanT = typename std::decay<T>::type>
types::Ok<CleanT> Ok(T&& val) {
    return types::Ok<CleanT>(std::forward<T>(val));
}

/// @brief 构造无值成功（用于 Result<void, E>）：`return Ok();`。
inline types::Ok<void> Ok() {
    return types::Ok<void>();
}

/// @brief 构造错误值：`return Err(std::string("bad"));`。
template<typename E, typename CleanE = typename std::decay<E>::type>
types::Err<CleanE> Err(E&& val) {
    return types::Err<CleanE>(std::forward<E>(val));
}

template<typename T, typename E> struct Result;

namespace details {

template<typename ...> struct void_t { typedef void type; };

namespace impl {
    template<typename Func> struct result_of;

    template<typename Ret, typename Cls, typename... Args>
    struct result_of<Ret (Cls::*)(Args...)> : public result_of<Ret (Args...)> { };

    template<typename Ret, typename Cls, typename... Args>
    struct result_of<Ret (Cls::*)(Args...) const> : public result_of<Ret (Args...)> { };

    template<typename Ret, typename... Args>
    struct result_of<Ret (Args...)> {
        typedef Ret type;
    };
}

template<typename Func>
struct result_of : public impl::result_of<decltype(&Func::operator())> { };

template<typename Ret, typename Cls, typename... Args>
struct result_of<Ret (Cls::*) (Args...) const> {
    typedef Ret type;
};

template<typename Ret, typename... Args>
struct result_of<Ret (*)(Args...)> {
    typedef Ret type;
};

template<typename R>
struct ResultOkType { typedef typename std::decay<R>::type type; };

template<typename T, typename E>
struct ResultOkType<Result<T, E>> {
    typedef T type;
};

template<typename R>
struct ResultErrType { typedef R type; };

template<typename T, typename E>
struct ResultErrType<Result<T, E>> {
    typedef typename std::remove_reference<E>::type type;
};

template<typename R> struct IsResult : public std::false_type { };
template<typename T, typename E>
struct IsResult<Result<T, E>> : public std::true_type { };

namespace ok {

namespace impl {

template<typename T> struct Map;

template<typename Ret, typename Cls, typename... Args>
struct Map<Ret (Cls::*)(Args...) const> : public Map<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Map<Ret (Cls::*)(Args...)> : public Map<Ret (Args...)> { };

// General implementation
template<typename Ret, typename Arg>
struct Map<Ret (Arg)> {

    static_assert(!IsResult<Ret>::value,
            "Can not map a callback returning a Result, use andThen instead");

    template<typename T, typename E, typename Func>
    static Result<Ret, E> map(const Result<T, E>& result, Func func) {

        static_assert(
                std::is_same<T, Arg>::value ||
                std::is_convertible<T, Arg>::value,
                "Incompatible types detected");

        if (result.is_ok()) {
            auto res = func(result.storage().template get<T>());
            return types::Ok<Ret>(std::move(res));
        }

        return types::Err<E>(result.storage().template get<E>());
    }
};

// Specialization for callback returning void
template<typename Arg>
struct Map<void (Arg)> {

    template<typename T, typename E, typename Func>
    static Result<void, E> map(const Result<T, E>& result, Func func) {

        if (result.is_ok()) {
            func(result.storage().template get<T>());
            return types::Ok<void>();
        }

        return types::Err<E>(result.storage().template get<E>());
    }
};

// Specialization for a void Result
template<typename Ret>
struct Map<Ret (void)> {

    template<typename T, typename E, typename Func>
    static Result<Ret, E> map(const Result<T, E>& result, Func func) {
        static_assert(std::is_same<T, void>::value,
                "Can not map a void callback on a non-void Result");

        if (result.is_ok()) {
            auto ret = func();
            return types::Ok<Ret>(std::move(ret));
        }

        return types::Err<E>(result.storage().template get<E>());
    }
};

// Specialization for callback returning void on a void Result
template<>
struct Map<void (void)> {

    template<typename T, typename E, typename Func>
    static Result<void, E> map(const Result<T, E>& result, Func func) {
        static_assert(std::is_same<T, void>::value,
                "Can not map a void callback on a non-void Result");

        if (result.is_ok()) {
            func();
            return types::Ok<void>();
        }

        return types::Err<E>(result.storage().template get<E>());
    }
};

// General specialization for a callback returning a Result
template<typename U, typename E, typename Arg>
struct Map<Result<U, E> (Arg)> {

    template<typename T, typename Func>
    static Result<U, E> map(const Result<T, E>& result, Func func) {
        static_assert(
                std::is_same<T, Arg>::value ||
                std::is_convertible<T, Arg>::value,
                "Incompatible types detected");

        if (result.is_ok()) {
            auto res = func(result.storage().template get<T>());
            return res;
        }

        return types::Err<E>(result.storage().template get<E>());
    }
};

// Specialization for a void callback returning a Result
template<typename U, typename E>
struct Map<Result<U, E> (void)> {

    template<typename T, typename Func>
    static Result<U, E> map(const Result<T, E>& result, Func func) {
        static_assert(std::is_same<T, void>::value, "Can not call a void-callback on a non-void Result");

        if (result.is_ok()) {
            auto res = func();
            return res;
        }

        return types::Err<E>(result.storage().template get<E>());
    }

};

} // namespace impl

template<typename Func> struct Map : public impl::Map<decltype(&Func::operator())> { };

template<typename Ret, typename... Args>
struct Map<Ret (*) (Args...)> : public impl::Map<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Map<Ret (Cls::*) (Args...)> : public impl::Map<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Map<Ret (Cls::*) (Args...) const> : public impl::Map<Ret (Args...)> { };

template<typename Ret, typename... Args>
struct Map<std::function<Ret (Args...)>> : public impl::Map<Ret (Args...)> { };

} // namespace ok


namespace err {

namespace impl {

template<typename T> struct Map;

template<typename Ret, typename Cls, typename Arg>
struct Map<Ret (Cls::*)(Arg) const> {

    static_assert(!IsResult<Ret>::value,
            "Can not map a callback returning a Result, use and_then instead");

    template<typename T, typename E, typename Func>
    static Result<T, Ret> map(const Result<T, E>& result, Func func) {
        if (result.is_err()) {
            auto res = func(result.storage().template get<E>());
            return types::Err<Ret>(res);
        }

        return types::Ok<T>(result.storage().template get<T>());
    }

    template<typename E, typename Func>
    static Result<void, Ret> map(const Result<void, E>& result, Func func) {
        if (result.is_err()) {
            auto res = func(result.storage().template get<E>());
            return types::Err<Ret>(res);
        }

        return types::Ok<void>();
    }


};

} // namespace impl

template<typename Func> struct Map : public impl::Map<decltype(&Func::operator())> { };

} // namespace err;

namespace And {

namespace impl {

    template<typename Func> struct Then;

    template<typename Ret, typename... Args>
    struct Then<Ret (*)(Args...)> : public Then<Ret (Args...)> { };

    template<typename Ret, typename Cls, typename... Args>
    struct Then<Ret (Cls::*)(Args...)> : public Then<Ret (Args...)> { };

    template<typename Ret, typename Cls, typename... Args>
    struct Then<Ret (Cls::*)(Args...) const> : public Then<Ret (Args...)> { };

    template<typename Ret, typename Arg>
    struct Then<Ret (Arg)> {
        static_assert(std::is_same<Ret, void>::value,
                "then() should not return anything, use map() instead");

        template<typename T, typename E, typename Func>
        static Result<T, E> then(const Result<T, E>& result, Func func) {
            if (result.is_ok()) {
                func(result.storage().template get<T>());
            }
            return result;
        }
    };

    template<typename Ret>
    struct Then<Ret (void)> {
        static_assert(std::is_same<Ret, void>::value,
                "then() should not return anything, use map() instead");

        template<typename T, typename E, typename Func>
        static Result<T, E> then(const Result<T, E>& result, Func func) {
            static_assert(std::is_same<T, void>::value, "Can not call a void-callback on a non-void Result");

            if (result.is_ok()) {
                func();
            }

            return result;
        }
    };


} // namespace impl

template<typename Func>
struct Then : public impl::Then<decltype(&Func::operator())> { };

template<typename Ret, typename... Args>
struct Then<Ret (*) (Args...)> : public impl::Then<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Then<Ret (Cls::*)(Args...)> : public impl::Then<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Then<Ret (Cls::*)(Args...) const> : public impl::Then<Ret (Args...)> { };

} // namespace And

namespace Or {

namespace impl {

    template<typename Func> struct Else;

    template<typename Ret, typename... Args>
    struct Else<Ret (*)(Args...)> : public Else<Ret (Args...)> { };

    template<typename Ret, typename Cls, typename... Args>
    struct Else<Ret (Cls::*)(Args...)> : public Else<Ret (Args...)> { };

    template<typename Ret, typename Cls, typename... Args>
    struct Else<Ret (Cls::*)(Args...) const> : public Else<Ret (Args...)> { };

    template<typename T, typename F, typename Arg>
    struct Else<Result<T, F> (Arg)> {

        template<typename E, typename Func>
        static Result<T, F> orElse(const Result<T, E>& result, Func func) {
            static_assert(
                    std::is_same<E, Arg>::value ||
                    std::is_convertible<E, Arg>::value,
                    "Incompatible types detected");

            if (result.is_err()) {
                auto res = func(result.storage().template get<E>());
                return res;
            }

            return types::Ok<T>(result.storage().template get<T>());
        }

        template<typename E, typename Func>
        static Result<void, F> orElse(const Result<void, E>& result, Func func) {
            if (result.is_err()) {
                auto res = func(result.storage().template get<E>());
                return res;
            }

            return types::Ok<void>();
        }

    };

    template<typename T, typename F>
    struct Else<Result<T, F> (void)> {

        template<typename E, typename Func>
        static Result<T, F> orElse(const Result<T, E>& result, Func func) {
            static_assert(std::is_same<T, void>::value,
                    "Can not call a void-callback on a non-void Result");

            if (result.is_err()) {
                auto res = func();
                return res;
            }

            return types::Ok<T>(result.storage().template get<T>());
        }

        template<typename E, typename Func>
        static Result<void, F> orElse(const Result<void, E>& result, Func func) {
            if (result.is_err()) {
                auto res = func();
                return res;
            }

            return types::Ok<void>();
        }

    };

} // namespace impl

template<typename Func>
struct Else : public impl::Else<decltype(&Func::operator())> { };

template<typename Ret, typename... Args>
struct Else<Ret (*) (Args...)> : public impl::Else<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Else<Ret (Cls::*)(Args...)> : public impl::Else<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Else<Ret (Cls::*)(Args...) const> : public impl::Else<Ret (Args...)> { };

} // namespace Or

namespace Other {

namespace impl {

    template<typename Func> struct Wise;

    template<typename Ret, typename... Args>
    struct Wise<Ret (*)(Args...)> : public Wise<Ret (Args...)> { };

    template<typename Ret, typename Cls, typename... Args>
    struct Wise<Ret (Cls::*)(Args...)> : public Wise<Ret (Args...)> { };

    template<typename Ret, typename Cls, typename... Args>
    struct Wise<Ret (Cls::*)(Args...) const> : public Wise<Ret (Args...)> { };

    template<typename Ret, typename Arg>
    struct Wise<Ret (Arg)> {

        template<typename T, typename E, typename Func>
        static Result<T, E> otherwise(const Result<T, E>& result, Func func) {
            static_assert(
                    std::is_same<E, Arg>::value ||
                    std::is_convertible<E, Arg>::value,
                    "Incompatible types detected");

            static_assert(std::is_same<Ret, void>::value,
                    "callback should not return anything, use map_error() for that");

            if (result.is_err()) {
                func(result.storage().template get<E>());
            }
            return result;
        }

    };

} // namespace impl

template<typename Func>
struct Wise : public impl::Wise<decltype(&Func::operator())> { };

template<typename Ret, typename... Args>
struct Wise<Ret (*) (Args...)> : public impl::Wise<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Wise<Ret (Cls::*)(Args...)> : public impl::Wise<Ret (Args...)> { };

template<typename Ret, typename Cls, typename... Args>
struct Wise<Ret (Cls::*)(Args...) const> : public impl::Wise<Ret (Args...)> { };

} // namespace Other

template<typename T, typename E, typename Func,
         typename Ret =
            Result<
                typename details::ResultOkType<
                    typename details::result_of<Func>::type
                >::type,
            E>
        >
Ret map(const Result<T, E>& result, Func func) {
    return ok::Map<Func>::map(result, func);
}

template<typename T, typename E, typename Func,
         typename Ret =
            Result<T,
                typename details::ResultErrType<
                    typename details::result_of<Func>::type
                >::type
            >
        >
Ret map_error(const Result<T, E>& result, Func func) {
    return err::Map<Func>::map(result, func);
}

template<typename T, typename E, typename Func>
Result<T, E> then(const Result<T, E>& result, Func func) {
    return And::Then<Func>::then(result, func);
}

template<typename T, typename E, typename Func>
Result<T, E> otherwise(const Result<T, E>& result, Func func) {
    return Other::Wise<Func>::otherwise(result, func);
}

template<typename T, typename E, typename Func,
    typename Ret =
        Result<T,
            typename details::ResultErrType<
                typename details::result_of<Func>::type
            >::type
       >
>
Ret or_else(const Result<T, E>& result, Func func) {
    return Or::Else<Func>::orElse(result, func);
}

struct ok_tag { };
struct err_tag { };

template<typename T, typename E>
struct Storage {
    static constexpr size_t Size = sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E);
    static constexpr size_t Align = sizeof(T) > sizeof(E) ? alignof(T) : alignof(E);

    typedef typename std::aligned_storage<Size, Align>::type type;

    Storage()
        : initialized_(false)
    { }

    void construct(types::Ok<T> ok)
    {
        new (&storage_) T(std::move(ok.val));
        initialized_ = true;
    }
    void construct(types::Err<E> err)
    {
        new (&storage_) E(std::move(err.val));
        initialized_ = true;
    }

    template<typename U>
    void rawConstruct(U&& val) {
        typedef typename std::decay<U>::type CleanU;

        new (&storage_) CleanU(std::forward<U>(val));
        initialized_ = true;
    }

    template<typename U>
    const U& get() const {
        return *reinterpret_cast<const U *>(&storage_);
    }

    template<typename U>
    U& get() {
        return *reinterpret_cast<U *>(&storage_);
    }

    void destroy(ok_tag) {
        if (initialized_) {
            get<T>().~T();
            initialized_ = false;
        }
    }

    void destroy(err_tag) {
        if (initialized_) {
            get<E>().~E();
            initialized_ = false;
        }
    }

    type storage_;
    bool initialized_;
};

template<typename E>
struct Storage<void, E> {
    typedef typename std::aligned_storage<sizeof(E), alignof(E)>::type type;

    void construct(types::Ok<void>)
    {
        initialized_ = true;
    }

    void construct(types::Err<E> err)
    {
        new (&storage_) E(std::move(err.val));
        initialized_ = true;
    }

    template<typename U>
    void rawConstruct(U&& val) {
        typedef typename std::decay<U>::type CleanU;

        new (&storage_) CleanU(std::forward<U>(val));
        initialized_ = true;
    }

    void destroy(ok_tag) { initialized_ = false; }
    void destroy(err_tag) {
        if (initialized_) {
            get<E>().~E(); initialized_ = false;
        }
    }

    template<typename U>
    const U& get() const {
        return *reinterpret_cast<const U *>(&storage_);
    }

    template<typename U>
    U& get() {
        return *reinterpret_cast<U *>(&storage_);
    }

    type storage_;
    bool initialized_;
};

template<typename T, typename E>
struct Constructor {

    static void move(Storage<T, E>&& src, Storage<T, E>& dst, ok_tag) {
        dst.rawConstruct(std::move(src.template get<T>()));
        src.destroy(ok_tag());
    }

    static void copy(const Storage<T, E>& src, Storage<T, E>& dst, ok_tag) {
        dst.rawConstruct(src.template get<T>());
    }

    static void move(Storage<T, E>&& src, Storage<T, E>& dst, err_tag) {
        dst.rawConstruct(std::move(src.template get<E>()));
        src.destroy(err_tag());
    }

    static void copy(const Storage<T, E>& src, Storage<T, E>& dst, err_tag) {
        dst.rawConstruct(src.template get<E>());
    }
};

template<typename E>
struct Constructor<void, E> {
    static void move(Storage<void, E>&& src, Storage<void, E>& dst, ok_tag) {
    }

    static void copy(const Storage<void, E>& src, Storage<void, E>& dst, ok_tag) {
    }

    static void move(Storage<void, E>&& src, Storage<void, E>& dst, err_tag) {
        dst.rawConstruct(std::move(src.template get<E>()));
        src.destroy(err_tag());
    }

    static void copy(const Storage<void, E>& src, Storage<void, E>& dst, err_tag) {
        dst.rawConstruct(src.template get<E>());
    }
};

} // namespace details

namespace concepts {

template<typename T, typename = void> struct EqualityComparable : std::false_type { };

template<typename T>
struct EqualityComparable<T,
typename std::enable_if<
    true,
    typename details::void_t<decltype(std::declval<T>() == std::declval<T>())>::type
    >::type
> : std::true_type
{
};


} // namespace concepts

/// @brief 持有成功值 T 或错误值 E 之一，二选一。用 Ok()/Err() 构造，is_ok()/is_err() 判别。
template<typename T, typename E>
struct Result {

    static_assert(!std::is_same<E, void>::value, "void error type is not allowed");

    typedef details::Storage<T, E> storage_type;

    Result(types::Ok<T> ok)
        : ok_(true)
    {
        storage_.construct(std::move(ok));
    }

    Result(types::Err<E> err)
        : ok_(false)
    {
        storage_.construct(std::move(err));
    }

    Result(Result&& other) {
        if (other.is_ok()) {
            details::Constructor<T, E>::move(std::move(other.storage_), storage_, details::ok_tag());
            ok_ = true;
        } else {
            details::Constructor<T, E>::move(std::move(other.storage_), storage_, details::err_tag());
            ok_ = false;
        }
    }

    Result(const Result& other) {
        if (other.is_ok()) {
            details::Constructor<T, E>::copy(other.storage_, storage_, details::ok_tag());
            ok_ = true;
        } else {
            details::Constructor<T, E>::copy(other.storage_, storage_, details::err_tag());
            ok_ = false;
        }
    }

    ~Result() {
        if (ok_)
            storage_.destroy(details::ok_tag());
        else
            storage_.destroy(details::err_tag());
    }

    /// 是否为成功。
    bool is_ok() const {
        return ok_;
    }

    /// 是否为错误。
    bool is_err() const {
        return !ok_;
    }

    /// @brief 成功则取值；失败则打印 str 到 stderr 并 std::terminate。
    T expect(const char* str) const {
        if (!is_ok()) {
            std::fprintf(stderr, "%s\n", str);
            std::terminate();
        }
        return expect_impl(std::is_same<T, void>());
    }

    /// @brief 成功值经 func 变换得到新 Result<U,E>；错误原样透传。func 不应返回 Result（那用 and_then）。
    template<typename Func,
             typename Ret =
                Result<
                    typename details::ResultOkType<
                        typename details::result_of<Func>::type
                    >::type,
                E>
            >
    Ret map(Func func) const {
        return details::map(*this, func);
    }

    /// @brief 错误值经 func 变换得到新错误类型；成功原样透传。
    template<typename Func,
         typename Ret =
             Result<T,
                typename details::ResultErrType<
                    typename details::result_of<Func>::type
                >::type
            >
    >
    Ret map_error(Func func) const {
        return details::map_error(*this, func);
    }

    /// @brief 成功时执行 func 副作用（func 返回 void），返回原 Result。
    template<typename Func>
    Result<T, E> then(Func func) const {
        return details::then(*this, func);
    }

    /// @brief 错误时执行 func 副作用（func 返回 void），返回原 Result。
    template<typename Func>
    Result<T, E> otherwise(Func func) const {
        return details::otherwise(*this, func);
    }

    /// @brief 错误时用 func 恢复为另一个 Result（func 返回 Result）；成功透传。
    template<typename Func,
        typename Ret =
            Result<T,
                typename details::ResultErrType<
                    typename details::result_of<Func>::type
                >::type
           >
    >
    Ret or_else(Func func) const {
        return details::or_else(*this, func);
    }

    /// @brief 成功时链式调用 func 返回另一个 Result（func 必须返回 Result）；错误透传。非 void 版本。
    template<typename Func,
        typename Ret = typename details::result_of<Func>::type,
        typename U = T>
    typename std::enable_if<
        !std::is_same<U, void>::value,
        Ret
    >::type
    and_then(Func func) const {
        static_assert(details::IsResult<Ret>::value,
            "and_then expects a function returning a Result");
        if (is_ok()) {
            return func(storage().template get<T>());
        }
        return types::Err<E>(storage().template get<E>());
    }

    /// @copydoc and_then() —— Result<void,E> 的重载（func 无参）。
    template<typename Func,
        typename Ret = typename details::result_of<Func>::type,
        typename U = T>
    typename std::enable_if<
        std::is_same<U, void>::value,
        Ret
    >::type
    and_then(Func func) const {
        static_assert(details::IsResult<Ret>::value,
            "and_then expects a function returning a Result");
        if (is_ok()) {
            return func();
        }
        return types::Err<E>(storage().template get<E>());
    }

    /// @brief 暴露内部存储，主要供 TRY 宏和底层工具使用，业务代码不应直接用。
    storage_type& storage() {
        return storage_;
    }

    const storage_type& storage() const {
        return storage_;
    }

    /// @brief 成功则取值，失败则返回 defaultValue（不终止）。
    template<typename U = T>
    typename std::enable_if<
        !std::is_same<U, void>::value,
        U
    >::type
    unwrap_or(const U& defaultValue) const {
        if (is_ok()) {
            return storage().template get<U>();
        }
        return defaultValue;
    }

    /// @brief 成功则取值；失败则 std::terminate。确定是 Ok 时才用。
    template<typename U = T>
    typename std::enable_if<
        !std::is_same<U, void>::value,
        U
    >::type
    unwrap() const {
        if (is_ok()) {
            return storage().template get<U>();
        }

        std::fprintf(stderr, "Attempting to unwrap an error Result\n");
        std::terminate();
    }

    /// @brief 错误则取错误值；成功则 std::terminate。确定是 Err 时才用。
    E unwrap_err() const {
        if (is_err()) {
            return storage().template get<E>();
        }

        std::fprintf(stderr, "Attempting to unwrap_err an ok Result\n");
        std::terminate();
    }

private:
    T expect_impl(std::true_type) const { }
    T expect_impl(std::false_type) const { return storage_.template get<T>(); }

    bool ok_;
    storage_type storage_;
};

template<typename T, typename E>
bool operator==(const Result<T, E>& lhs, const Result<T, E>& rhs) {
    static_assert(concepts::EqualityComparable<T>::value, "T must be EqualityComparable for Result to be comparable");
    static_assert(concepts::EqualityComparable<E>::value, "E must be EqualityComparable for Result to be comparable");

    if (lhs.is_ok() && rhs.is_ok()) {
        return lhs.storage().template get<T>() == rhs.storage().template get<T>();
    }
    if (lhs.is_err() && rhs.is_err()) {
        return lhs.storage().template get<E>() == rhs.storage().template get<E>();
    }
    return false;
}

template<typename T, typename E>
bool operator==(const Result<T, E>& lhs, types::Ok<T> ok) {
    static_assert(concepts::EqualityComparable<T>::value, "T must be EqualityComparable for Result to be comparable");

    if (!lhs.is_ok()) return false;

    return lhs.storage().template get<T>() == ok.val;
}

template<typename E>
bool operator==(const Result<void, E>& lhs, const Result<void, E>& rhs) {
    if (lhs.is_ok() && rhs.is_ok()) return true;
    if (lhs.is_err() && rhs.is_err()) {
        return lhs.storage().template get<E>() == rhs.storage().template get<E>();
    }
    return false;
}

template<typename E>
bool operator==(const Result<void, E>& lhs, types::Ok<void>) {
    return lhs.is_ok();
}

template<typename T, typename E>
bool operator==(const Result<T, E>& lhs, types::Err<E> err) {
    static_assert(concepts::EqualityComparable<E>::value, "E must be EqualityComparable for Result to be comparable");
    if (!lhs.is_err()) return false;

    return lhs.storage().template get<E>() == err.val;
}

namespace details {

template<typename T, typename StorageT>
typename std::enable_if<std::is_same<T, void>::value, void>::type
try_take_ok(StorageT&) {
}

template<typename T, typename StorageT>
typename std::enable_if<!std::is_same<T, void>::value, T>::type
try_take_ok(StorageT& storage) {
    return std::move(storage.template get<T>());
}

} // namespace details

/// @brief 在返回 Result<*,E> 的函数里展开另一个 Result<T,E>：成功取出 Ok 值，失败提前 return Err。
/// @warning 依赖 GNU statement expression（`__extension__ ({...})`），仅 GCC/Clang 可用；
///          MSVC 不支持。需要 MSVC 兼容的代码请手动 if (res.is_err()) return ...。
#define TRY(...)                                                   \
    __extension__ ({                                               \
        auto res = __VA_ARGS__;                                    \
        if (!res.is_ok()) {                                         \
            typedef details::ResultErrType<decltype(res)>::type E; \
            return types::Err<E>(std::move(                         \
                res.storage().template get<E>()));                  \
        }                                                          \
        typedef details::ResultOkType<decltype(res)>::type T;      \
        details::try_take_ok<T>(res.storage());                    \
    })

}  // namespace ca::core

// 向后兼容：让 ca::Result 等可用。新代码应显式用 ca::core::。
namespace ca {
    using namespace ca::core;
}
