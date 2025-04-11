#pragma once

#include <vector>
#include <iostream>
#include <algorithm>
#include <functional>
#include <iterator>
#include <type_traits>

// 包装器类，用于链式调用
template<typename T>
class ChainableVector
{
public:
    ChainableVector(const std::vector<T>& vec)
        : vec_(vec)
    {}

    // 过滤操作
    ChainableVector<T> filter(std::function<bool(const T&)> predicate)
    {
        std::vector<T> filtered;
        std::copy_if(vec_.begin(), vec_.end(), std::back_inserter(filtered), predicate);
        return ChainableVector<T>(filtered);
    }

    // 映射操作
    template<typename U>
    ChainableVector<U> map(std::function<U(const T&)> transform)
    {
        std::vector<U> mapped;
        std::transform(vec_.begin(), vec_.end(), std::back_inserter(mapped), transform);
        return ChainableVector<U>(mapped);
    }

    // 收集操作，返回最终的向量
    std::vector<T> collect() { return vec_; }

private:
    std::vector<T> vec_;
};

// 辅助类型，用于检测value_type别名
template<typename Iterator, typename = void>
struct has_value_type : std::false_type
{};

template<typename Iterator>
struct has_value_type<Iterator, std::void_t<typename Iterator::value_type>> : std::true_type
{};

template<typename Container>
class Stream
{
public:
    Stream(Container cont)
        : container(std::move(cont))
    {}

    template<typename Func>
    Stream<Container> map(Func func)
    {
        Container result;
        result.reserve(container.size());
        std::transform(container.begin(), container.end(), std::back_inserter(result), func);
        return Stream<Container>(std::move(result));
    }

    template<typename Pred>
    Stream<Container> filter(Pred pred)
    {
        Container result;
        std::copy_if(container.begin(), container.end(), std::back_inserter(result), pred);
        return Stream<Container>(std::move(result));
    }

    Container collect() { return container; }



private:
    Container container;
};

template<typename ElementType, typename Iterator>
class LazyStream
{
public:
    using value_type = ElementType;
    using iterator   = Iterator;

private:
    iterator                              begin_itr;
    iterator                              end_itr;
    std::function<bool(value_type)>       filterPredicate;
    std::function<value_type(value_type)> mapFunction;

public:
    // 从迭代器范围构造流
    LazyStream(Iterator begin, Iterator end)
        : begin_itr(begin)
        , end_itr(end)
    {}

    // 中间操作：过滤（惰性）
    auto filter(std::function<bool(value_type)> predicate) -> LazyStream<value_type, iterator>&
    {
        filterPredicate = predicate;
        return *this;
    }

    // 中间操作：映射（惰性）
    auto map(std::function<value_type(value_type)> mapper) -> LazyStream<value_type, iterator>&
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

    auto forEach(std::function<void(value_type)> consumer) -> LazyStream<value_type, iterator>&
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

template<typename T, typename = void>
struct has_begin_end : std::false_type
{};

template<typename T>
struct has_begin_end<
    T, std::void_t<decltype(std::declval<T>().begin()), decltype(std::declval<T>().end())>>
    : std::true_type
{};

template<typename Container>
auto stream(Container container)
    -> std::enable_if_t<has_begin_end<Container>::value,
                        LazyStream<typename Container::value_type, decltype(container.begin())>>
{
    return LazyStream<typename Container::value_type, decltype(container.begin())>(
        container.begin(), container.end());
}

// 如果Container没有begin和end方法，则提供备用实现或错误信息
// example:
// int a;
// stream(a);
// error C2338: static_assert failed: 'Container must have begin() and end() methods'
template<typename Container>
auto stream(Container container) -> std::enable_if_t<!has_begin_end<Container>::value, void>
{
    static_assert(has_begin_end<Container>::value, "Container must have begin() and end() methods");
}
