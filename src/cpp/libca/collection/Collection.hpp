#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <type_traits>


template<typename T, typename Iter>
class Stream
{
public:
    using value_type = T;
    using iterator   = Iter;

private:
    iterator                              begin_itr;
    iterator                              end_itr;
    std::function<bool(value_type)>       filterPredicate;
    std::function<value_type(value_type)> mapFunction;

public:
    // 从迭代器范围构造流
    Stream(iterator begin, iterator end)
        : begin_itr(begin)
        , end_itr(end)
    {}

    // 中间操作：过滤（惰性）
    auto filter(std::function<bool(value_type)> predicate) -> Stream<value_type, iterator>&
    {
        filterPredicate = predicate;
        return *this;
    }

    // 中间操作：映射（惰性）
    auto map(std::function<value_type(value_type)> mapper) -> Stream<value_type, iterator>&
    {
        mapFunction = mapper;
        return *this;
    }

    // 终端操作：收集到容器（执行操作）
    template<typename Container>
    auto collect() -> Container
    {
        Container result;
        for (auto itr = begin_itr; itr != end_itr; ++itr) {
            if (!filterPredicate || filterPredicate(*itr)) {
                value_type value = *itr;
                if (mapFunction) {
                    value = mapFunction(value);
                }
                result.push_back(value);
            }
        }
        return result;
    }

    auto forEach(std::function<void(value_type)> consumer) -> Stream<value_type, iterator>&
    {
        for (auto itr = begin_itr; itr != end_itr; ++itr) {
            if (!filterPredicate || filterPredicate(*itr)) {
                value_type value = *itr;
                if (mapFunction) {
                    value = mapFunction(value);
                }
                consumer(value);
            }
        }
        return *this;
    }
};

// 检测begin方法
template<typename T, typename = void>
struct has_begin : std::false_type
{};

template<typename T>
struct has_begin<
    T, std::void_t<decltype(std::declval<T>().begin())>>
    : std::true_type
{};

// 检测end方法
template<typename T, typename = void>
struct has_end : std::false_type
{};

template<typename T>
struct has_end<
    T, std::void_t<decltype(std::declval<T>().end())>>
    : std::true_type
{};

template<typename T>
struct has_begin_end : std::conjunction<has_begin<T>, has_end<T>>
{};

// 检测value_type别名
template<typename Iter, typename = void>
struct has_value_type : std::false_type
{};

template<typename Iter>
struct has_value_type<Iter, std::void_t<typename Iter::value_type>> : std::true_type
{};

template<typename Container>
struct stream_container_required_traits : std::conjunction<has_begin_end<Container>, has_value_type<Container>> {};

template<typename Container>
auto stream(Container container)
    -> std::enable_if_t<stream_container_required_traits<Container>::value,
                        Stream<typename Container::value_type, decltype(container.begin())>>
{
    return Stream<typename Container::value_type, decltype(container.begin())>(container.begin(),
                                                                               container.end());
}

// 如果Container没有begin和end方法，则提供备用实现或错误信息
// example:
// int a;
// stream(a);
// error C2338: static_assert failed: 'Container must have begin() and end() methods'
template<typename Container>
auto stream(Container container) -> std::enable_if_t<!stream_container_required_traits<Container>::value, void>
{
    static_assert(has_begin_end<Container>::value, "Container must have begin() and end() methods");
}
