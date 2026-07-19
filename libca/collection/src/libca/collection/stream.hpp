/// @file stream.hpp
/// @brief Stream — 基于迭代器范围的惰性流式处理（header-only 模板）。
///
/// 命名空间 ca::collection。提供类似 Java Stream 的 filter/map/collect/forEach
/// 入口：filter 与 map 仅暂存谓词，真正求值发生在 forEach / collect 等终止操作。

#pragma once

#include <functional>
#include <type_traits>

namespace ca::collection {

/// @brief 基于迭代器范围 `[begin, end)` 的惰性流。
/// @tparam T   元素值类型。
/// @tparam Iter 迭代器类型，需支持 `*it` 和 `++it`。
///
/// filter() / map() 暂存谓词不立即求值；forEach() / collect() 才遍历并应用。
/// 当前实现仅支持单段 filter + 单段 map（后设置覆盖前设置）。
template<typename T, typename Iter>
class Stream {
public:
    using value_type = T;
    using iterator = Iter;

    /// @brief 用迭代器范围构造流。
    Stream(iterator begin, iterator end)
        : begin_(begin), end_(end) {}

    /// @brief 设置过滤谓词（惰性，不立即遍历）。
    /// @return *this，便于链式调用。
    Stream& filter(std::function<bool(value_type)> predicate) {
        filter_ = std::move(predicate);
        return *this;
    }

    /// @brief 设置映射函数（惰性，不立即遍历）。
    /// @return *this，便于链式调用。
    Stream& map(std::function<value_type(value_type)> mapper) {
        map_ = std::move(mapper);
        return *this;
    }

    /// @brief 遍历并应用 consumer，对每个通过 filter 的元素执行操作。
    /// @return *this。
    Stream& forEach(std::function<void(value_type)> consumer) {
        for (auto it = begin_; it != end_; ++it) {
            if (!filter_ || filter_(*it)) {
                auto val = *it;
                if (map_) val = map_(val);
                consumer(val);
            }
        }
        return *this;
    }

    /// @brief 终止操作：把通过 filter 的元素（经 map 后）收集到 Container。
    /// @tparam Container 目标容器，需支持 push_back。
    /// @return 填充后的容器。
    template<typename Container>
    Container collect() {
        Container result;
        for (auto it = begin_; it != end_; ++it) {
            if (!filter_ || filter_(*it)) {
                auto val = *it;
                if (map_) val = map_(val);
                result.push_back(std::move(val));
            }
        }
        return result;
    }

private:
    iterator begin_;
    iterator end_;
    std::function<bool(value_type)> filter_;
    std::function<value_type(value_type)> map_;
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
}

/// @brief 从可变容器创建 Stream（SFINAE：要求容器有 begin/end/value_type）。
template<typename Container>
auto stream(Container& container)
    -> std::enable_if_t<detail::container_traits<Container>::value,
                        Stream<typename Container::value_type, decltype(container.begin())>>
{
    return Stream<typename Container::value_type, decltype(container.begin())>(
        container.begin(), container.end());
}

/// @brief 从 const 容器创建 Stream，元素为 const T。
template<typename Container>
auto stream(const Container& container)
    -> std::enable_if_t<detail::container_traits<Container>::value,
                        Stream<const typename Container::value_type, decltype(container.begin())>>
{
    return Stream<const typename Container::value_type, decltype(container.begin())>(
        container.begin(), container.end());
}

} // namespace ca::collection
