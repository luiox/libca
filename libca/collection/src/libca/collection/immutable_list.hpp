///
/// @brief ImmutableList — 不可变列表
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::collection，创建后不可修改的只读列表
///       所有实现均在头文件（模板），依赖 C++17
///

#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ca::collection {

namespace detail {
    template<typename First, typename...>
    struct first_type { using type = First; };
}

/// 不可变列表 — 构造后内容只读，支持范围 for 和随机访问
template<typename T>
class ImmutableList {
public:
    /// 从多个值构造
    template<typename... Args,
             typename = std::enable_if_t<(std::is_convertible_v<Args, T> && ...)>>
    explicit ImmutableList(Args&&... args)
        : size_(sizeof...(Args))
    {
        auto raw = std::make_unique<T[]>(size_);
        size_t idx = 0;
        ((raw[idx++] = std::forward<Args>(args)), ...);
        data_ = std::move(raw);  // unique_ptr<T[]> → unique_ptr<const T[]>
    }

    /// 空列表
    ImmutableList() : size_(0), data_(nullptr) {}

    ImmutableList(const ImmutableList&) = delete;
    ImmutableList& operator=(const ImmutableList&) = delete;

    ImmutableList(ImmutableList&&) noexcept = default;
    ImmutableList& operator=(ImmutableList&&) noexcept = default;

    /// 随机访问
    const T& operator[](size_t index) const {
        if (index >= size_) throw std::out_of_range("ImmutableList: index out of range");
        return data_[index];
    }

    const T* begin() const noexcept { return data_.get(); }
    const T* end() const noexcept { return data_.get() + size_; }

    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    /// 追加元素并返回新列表（原列表不变）
    ImmutableList appended(const T& value) const {
        // 使用 mutable 数组用于构造，移交时转为 const
        auto raw = std::make_unique<T[]>(size_ + 1);
        for (size_t i = 0; i < size_; ++i) raw[i] = data_[i];
        raw[size_] = value;
        return ImmutableList(std::move(raw), size_ + 1);
    }

    /// 工厂方法：从多参数创建（自动推导类型）
    template<typename... Args>
    static auto of(Args&&... args)
        -> ImmutableList<typename detail::first_type<Args...>::type>
    {
        return ImmutableList<typename detail::first_type<Args...>::type>(
            std::forward<Args>(args)...);
    }

    /// 创建空列表
    static ImmutableList createEmpty() { return ImmutableList(); }

private:
    size_t size_{0};
    std::unique_ptr<const T[]> data_;

    // 用于 appended 的私有构造
    ImmutableList(std::unique_ptr<const T[]>&& data, size_t size)
        : size_(size), data_(std::move(data)) {}
};

} // namespace ca::collection
