///
/// @brief Stream — 容器流式操作
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::collection，提供类似 Java Stream 的惰性 filter/map/collect/forEach
///       所有实现均在头文件（模板），依赖 C++17
///

#pragma once

#include <functional>
#include <type_traits>

namespace ca::collection {

/// 基于迭代器范围的惰性流式处理
template<typename T, typename Iter>
class Stream {
public:
    using value_type = T;
    using iterator = Iter;

    Stream(iterator begin, iterator end)
        : begin_(begin), end_(end) {}

    /// 过滤（惰性）
    Stream& filter(std::function<bool(value_type)> predicate) {
        filter_ = std::move(predicate);
        return *this;
    }

    /// 映射（惰性）
    Stream& map(std::function<value_type(value_type)> mapper) {
        map_ = std::move(mapper);
        return *this;
    }

    /// 遍历
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

    /// 收集到容器
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

/// 从容器创建 Stream（SFINAE：要求容器有 begin/end/value_type）
template<typename Container>
auto stream(Container& container)
    -> std::enable_if_t<detail::container_traits<Container>::value,
                        Stream<typename Container::value_type, decltype(container.begin())>>
{
    return Stream<typename Container::value_type, decltype(container.begin())>(
        container.begin(), container.end());
}

/// 从 const 容器创建 Stream
template<typename Container>
auto stream(const Container& container)
    -> std::enable_if_t<detail::container_traits<Container>::value,
                        Stream<const typename Container::value_type, decltype(container.begin())>>
{
    return Stream<const typename Container::value_type, decltype(container.begin())>(
        container.begin(), container.end());
}

} // namespace ca::collection
