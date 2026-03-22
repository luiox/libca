#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <memory>
#include <stdexcept>

namespace ca {

template<typename First, typename... Args>
struct FirstType
{
    using type = First;
};

template<typename T>
class ImmutableList
{
private:
    const size_t               size_;
    std::unique_ptr<const T[]> data_;   // 双重不可变保障

    // 禁用拷贝操作
    ImmutableList(const ImmutableList&)            = delete;
    ImmutableList& operator=(const ImmutableList&) = delete;

public:
    template<typename... Args, typename = std::enable_if_t<(std::is_convertible_v<Args, T> && ...)>>
    ImmutableList(Args&&... args)
        : size_{sizeof...(Args)}
        , data_{std::make_unique<const T[]>(size_)}
    {
        T*     temp  = const_cast<T*>(data_.get());   // 安全：构造阶段独占所有权
        size_t index = 0;
        ((temp[index++] = std::forward<Args>(args)), ...);   // C++17折叠表达式
    }

    // 允许移动语义
    ImmutableList(ImmutableList&&) noexcept            = default;
    ImmutableList& operator=(ImmutableList&&) noexcept = default;

    const T& operator[](size_t index) const
    {
        if (index >= size_)
            throw std::out_of_range("Index overflow");
        return data_[index];
    }

    // 支持范围for循环
    const T* begin() const noexcept { return data_.get(); }
    const T* end() const noexcept { return data_.get() + size_; }

    size_t size() const noexcept { return size_; }

    // 创建新列表（结构共享优化）
    ImmutableList<T> appended(const T& value) const
    {
        auto new_data = std::make_unique<const T[]>(size_ + 1);
        std::copy(data_.get(), data_.get() + size_, new_data.get());
        new_data[size_] = value;
        return ImmutableList<T>(std::move(new_data), size_ + 1);
    }

    template<typename... Args>
    static auto of(Args&&... args) -> ImmutableList<typename FirstType<Args...>::type>
    {
        return create(std::forward<Args>(args)...);
    }

    // 空列表构造函数
    explicit ImmutableList(std::nullptr_t)
        : size_(0)
        , data_(nullptr)
    {}

    // 空列表工厂方法
    static ImmutableList<T> empty() { return ImmutableList<T>(0, nullptr); }

private:
    ImmutableList(std::unique_ptr<const T[]>&& data, size_t size)
        : size_(size)
        , data_(std::move(data))
    {}

    // 实际构造器
    template<typename... Args>
    static ImmutableList<T> create(Args&&... args)
    {
        static_assert(sizeof...(Args) > 0, "At least one element required");
        static_assert((std::is_convertible_v<Args, T> && ...),
                      "All arguments must be convertible to T");

        auto   data  = std::make_unique<const T[]>(sizeof...(Args));
        T*     temp  = const_cast<T*>(data.get());
        size_t index = 0;
        ((temp[index++] = std::forward<Args>(args)), ...);

        return ImmutableList<T>(sizeof...(Args), std::move(data));
    }

    // 私有构造器
    ImmutableList(size_t size, std::unique_ptr<const T[]> data)
        : size_(size)
        , data_(std::move(data))
    {}
};

// 支持ImmutableList::of(1,2,3)的自动类型推导
template<typename... Args>
ImmutableList(Args&&... args) -> ImmutableList<typename FirstType<Args...>::type>;

// test
// template<typename T>
// class TestClass{
// public:
//     template<typename... Args>
//     static auto of(Args&&... args) -> TestClass<typename FirstType<Args...>::type>
//     {
//         return nullptr;
//     }

// };

}   // namespace ca
