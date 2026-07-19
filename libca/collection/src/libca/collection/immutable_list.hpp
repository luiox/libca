/// @file immutable_list.hpp
/// @brief ImmutableList — 构造后内容只读的列表（header-only 模板）。
///
/// 命名空间 ca::collection。底层数组用 `std::unique_ptr<const T[]>` 持有，
/// 无拷贝（只支持移动），通过 appended() 产生新列表实现"逻辑可变"。

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

/// @brief 构造后内容只读的列表，支持范围 for 和随机访问。
/// @tparam T 元素类型。
///
/// 内容通过 `const T[]` 持有，构造后无法原地修改；需要扩展时使用 `appended()`
/// 返回新列表。越界下标访问抛出 `std::out_of_range`。
template<typename T>
class ImmutableList {
public:
    /// @brief 从多个可转换为 T 的值构造列表。
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

    /// @brief 构造空列表。
    ImmutableList() : size_(0), data_(nullptr) {}

    ImmutableList(const ImmutableList&) = delete;
    ImmutableList& operator=(const ImmutableList&) = delete;

    ImmutableList(ImmutableList&&) noexcept = default;
    ImmutableList& operator=(ImmutableList&&) noexcept = default;

    /// @brief 随机访问（只读）。
    /// @throws std::out_of_range 当 index >= size()。
    const T& operator[](size_t index) const {
        if (index >= size_) throw std::out_of_range("ImmutableList: index out of range");
        return data_[index];
    }

    const T* begin() const noexcept { return data_.get(); }
    const T* end() const noexcept { return data_.get() + size_; }

    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    /// @brief 追加元素并返回新列表，原列表保持不变。
    /// @return 新构造的 ImmutableList，长度比当前大 1。
    ImmutableList appended(const T& value) const {
        // 使用 mutable 数组用于构造，移交时转为 const
        auto raw = std::make_unique<T[]>(size_ + 1);
        for (size_t i = 0; i < size_; ++i) raw[i] = data_[i];
        raw[size_] = value;
        return ImmutableList(std::move(raw), size_ + 1);
    }

    /// @brief 工厂方法：从多个值创建列表，自动推导元素类型。
    template<typename... Args>
    static auto of(Args&&... args)
        -> ImmutableList<typename detail::first_type<Args...>::type>
    {
        return ImmutableList<typename detail::first_type<Args...>::type>(
            std::forward<Args>(args)...);
    }

    /// @brief 创建空列表。
    static ImmutableList createEmpty() { return ImmutableList(); }

private:
    size_t size_{0};
    std::unique_ptr<const T[]> data_;

    // 用于 appended 的私有构造
    ImmutableList(std::unique_ptr<const T[]>&& data, size_t size)
        : size_(size), data_(std::move(data)) {}
};

} // namespace ca::collection
