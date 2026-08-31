/// @file stream.hpp
/// @brief Stream — 基于迭代器范围的惰性流水线（header-only 模板）。
///
/// 命名空间 ca::collection。Rust 迭代器风格的适配器链：
/// `stream(c).filter(p1).filter(p2).map(f).take(n).collect<Container>()`。
/// 每个适配器返回新的流水线节点（按值持有上游），filter/map 可任意叠加；
/// for_each / collect / count / reduce / any / all 是终止操作，触发一次遍历。

#pragma once

#include <type_traits>
#include <utility>

#include "libca/core/datatype.hpp"
#include "libca/core/option.hpp"

namespace ca::collection {

// core 的 Option 族在本命名空间内引入 using 声明，避免非限定名经 ca 顶层
// 别名（ca::Option）与 ca::core::Option 在依赖名查找下产生二义。
using ca::core::None;
using ca::core::Option;
using ca::core::Some;

// 前置声明：stream_base 适配器的返回类型（定义在 stream_base 之后）。
template<typename Parent, typename Pred>
class FilterStream;
template<typename Parent, typename F>
class MapStream;
template<typename Parent>
class TakeStream;
template<typename Parent>
class SkipStream;

/// @brief 所有流水线节点的公共基类（CRTP）：统一提供适配器与终止操作。
/// @tparam Derived 节点具体类型，须实现 `Option<value_type> next()` 拉取下一个元素。
/// @tparam T       流中元素的值类型（按值传出）。
///
/// 适配器（filter/map/take/skip）返回**新节点**并移动消费当前节点——与 Rust
/// 迭代器适配器按值接管上游一致。流水线节点是一次性的：调用适配器后再使用
/// 旧节点属于 moved-from 状态，不要这样做。
template<typename Derived, typename T>
class stream_base {
public:
    /// @brief 流中元素的值类型。
    using value_type = T;

    // ---- 适配器（返回新节点，移动消费 *this）----

    /// @brief 只放行满足 predicate 的元素。
    template<typename Pred>
    FilterStream<Derived, std::decay_t<Pred>> filter(Pred&& predicate) {
        return FilterStream<Derived, std::decay_t<Pred>>(std::move(derived()),
                                                         std::forward<Pred>(predicate));
    }

    /// @brief 对每个元素应用 mapper，可改变元素类型。
    template<typename F>
    MapStream<Derived, std::decay_t<F>> map(F&& mapper) {
        return MapStream<Derived, std::decay_t<F>>(std::move(derived()), std::forward<F>(mapper));
    }

    /// @brief 最多放行前 count 个元素。
    TakeStream<Derived> take(usize count) {
        return TakeStream<Derived>(std::move(derived()), count);
    }

    /// @brief 跳过前 count 个元素。
    SkipStream<Derived> skip(usize count) {
        return SkipStream<Derived>(std::move(derived()), count);
    }

    // ---- 终止操作（单次遍历求值，消费整个流水线）----

    /// @brief 对每个元素执行 consumer。
    template<typename F>
    void for_each(F&& consumer) {
        auto& self = derived();
        while (auto item = self.next()) {
            consumer(std::move(item.unwrap()));
        }
    }

    /// @brief 把元素收集到 Container（需支持 push_back）。
    template<typename Container>
    Container collect() {
        Container result;
        auto& self = derived();
        while (auto item = self.next()) {
            result.push_back(std::move(item.unwrap()));
        }
        return result;
    }

    /// @brief 统计元素个数。
    usize count() {
        auto& self = derived();
        usize total = 0;
        while (self.next()) {
            ++total;
        }
        return total;
    }

    /// @brief 从 init 起折叠所有元素（对标 Rust Iterator::fold）。
    template<typename Init, typename Op>
    Init reduce(Init init, Op&& op) {
        auto& self = derived();
        while (auto item = self.next()) {
            init = op(std::move(init), std::move(item.unwrap()));
        }
        return init;
    }

    /// @brief 存在任一元素满足 predicate 则 true（短路求值）。
    template<typename Pred>
    bool any(Pred&& predicate) {
        auto& self = derived();
        while (auto item = self.next()) {
            if (predicate(item.unwrap())) {
                return true;
            }
        }
        return false;
    }

    /// @brief 所有元素都满足 predicate 则 true；空流为 true（短路求值）。
    template<typename Pred>
    bool all(Pred&& predicate) {
        auto& self = derived();
        while (auto item = self.next()) {
            if (!predicate(item.unwrap())) {
                return false;
            }
        }
        return true;
    }

private:
    Derived& derived() { return static_cast<Derived&>(*this); }
};

/// @brief 过滤节点：只放行满足 predicate 的元素。
template<typename Parent, typename Pred>
class FilterStream : public stream_base<FilterStream<Parent, Pred>, typename Parent::value_type> {
public:
    FilterStream(Parent&& parent, Pred predicate)
        : parent_(std::move(parent)), predicate_(std::move(predicate)) {}

    /// @brief 拉取下一个通过过滤的元素；上游耗尽返回 None。
    Option<typename FilterStream::value_type> next() {
        while (auto item = parent_.next()) {
            if (predicate_(item.unwrap())) {
                return item;
            }
        }
        return None;
    }

private:
    Parent parent_;
    Pred   predicate_;
};

/// @brief 映射节点：对上游元素应用 mapper，元素类型随 mapper 返回类型改变。
template<typename Parent, typename F>
class MapStream
    : public stream_base<MapStream<Parent, F>,
                         std::decay_t<std::invoke_result_t<F&, typename Parent::value_type&&>>> {
public:
    MapStream(Parent&& parent, F mapper)
        : parent_(std::move(parent)), mapper_(std::move(mapper)) {}

    /// @brief 拉取下一个映射结果；上游耗尽返回 None。
    Option<typename MapStream::value_type> next() {
        if (auto item = parent_.next()) {
            return Some(mapper_(std::move(item.unwrap())));
        }
        return None;
    }

private:
    Parent parent_;
    F      mapper_;
};

/// @brief 截断节点：最多放行前 limit 个元素，之后立即返回 None。
template<typename Parent>
class TakeStream : public stream_base<TakeStream<Parent>, typename Parent::value_type> {
public:
    TakeStream(Parent&& parent, usize limit)
        : parent_(std::move(parent)), remaining_(limit) {}

    /// @brief 拉取下一个元素；配额用尽或上游耗尽返回 None。
    Option<typename TakeStream::value_type> next() {
        if (remaining_ == 0) {
            return None;
        }
        --remaining_;
        return parent_.next();
    }

private:
    Parent parent_;
    usize  remaining_;
};

/// @brief 跳过节点：首个 next() 惰性丢弃前 count 个元素，其后透传上游。
template<typename Parent>
class SkipStream : public stream_base<SkipStream<Parent>, typename Parent::value_type> {
public:
    SkipStream(Parent&& parent, usize count)
        : parent_(std::move(parent)), to_skip_(count) {}

    /// @brief 拉取下一个元素（首次调用会先跳过前 count 个）；上游耗尽返回 None。
    Option<typename SkipStream::value_type> next() {
        while (to_skip_ > 0) {
            if (!parent_.next()) {
                return None;
            }
            --to_skip_;
        }
        return parent_.next();
    }

private:
    Parent parent_;
    usize  to_skip_;
};

/// @brief 基于迭代器范围 `[begin, end)` 的流水线源节点。
/// @tparam T   元素值类型（从迭代器解引用按值拷出）。
/// @tparam Iter 迭代器类型，需支持 `*it` 和 `++it`。
template<typename T, typename Iter>
class Stream : public stream_base<Stream<T, Iter>, T> {
public:
    /// @brief 用迭代器范围构造流。
    Stream(Iter begin, Iter end) : current_(begin), end_(end) {}

    /// @brief 拉取下一个元素；遍历结束返回 None。
    Option<T> next() {
        if (current_ == end_) {
            return None;
        }
        T value = *current_;
        ++current_;
        return Some(std::move(value));
    }

private:
    Iter current_;
    Iter end_;
};

// ============================================================================
// stream() 工厂函数
// ============================================================================

namespace detail {
    template<typename T, typename = void>
    struct has_begin : std::false_type {};
    template<typename T>
    struct has_begin<T, std::void_t<decltype(std::declval<T>().begin())>> : std::true_type {};

    template<typename T, typename = void>
    struct has_end : std::false_type {};
    template<typename T>
    struct has_end<T, std::void_t<decltype(std::declval<T>().end())>> : std::true_type {};

    template<typename T, typename = void>
    struct has_value_type : std::false_type {};
    template<typename T>
    struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {};

    template<typename T>
    struct container_traits
        : std::conjunction<has_begin<T>, has_end<T>, has_value_type<T>> {};
}   // namespace detail

/// @brief 从容器创建 Stream（SFINAE：要求容器有 begin/end/value_type）。
/// @details 元素按值拷出；const 容器同样产出可变的值副本。
template<typename Container>
auto stream(Container& container)
    -> std::enable_if_t<detail::container_traits<Container>::value,
                        Stream<std::decay_t<typename Container::value_type>, decltype(container.begin())>>
{
    using T = std::decay_t<typename Container::value_type>;
    return Stream<T, decltype(container.begin())>(container.begin(), container.end());
}

}   // namespace ca::collection
