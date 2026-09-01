#pragma once

#include "libca/core/datatype.hpp"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

/// @file array_list.hpp
/// @brief ArrayList — Rust-like 基础 API 的可变顺序容器。

namespace ca::collection {

/// @brief 基于 `std::vector` 的拥有型可变顺序容器。
///
/// `ArrayList` 提供接近 Rust/Java 常用集合语义的稳定入口：类型名贴近
/// Java `ArrayList`，方法名保持 snake_case。越界访问类 API 会抛出
/// `std::out_of_range`，`try_get()` / `first()` / `last()` 使用空指针表示不存在。
template<typename T>
class ArrayList
{
public:
    /// @brief 未找到元素时返回的哨兵值。
    static constexpr ca::usize npos = static_cast<ca::usize>(-1);

    using value_type     = T;
    using iterator       = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    ArrayList() = default;

    /// @brief 用初始化列表构造列表，保留输入顺序。
    ArrayList(std::initializer_list<T> values)
        : data_(values)
    {}

    /// @brief 创建并预留 capacity 容量的空列表。
    static ArrayList with_capacity(ca::usize capacity)
    {
        ArrayList out;
        out.reserve(capacity);
        return out;
    }

    /// @brief 从连续数组拷贝构造列表。
    /// @throws std::invalid_argument 当 length 非 0 且 data 为空。
    static ArrayList from_slice(const T* data, ca::usize length)
    {
        ArrayList out;
        out.extend_from_slice(data, length);
        return out;
    }

    /// @brief 返回元素数量。
    ca::usize len() const noexcept { return static_cast<ca::usize>(data_.size()); }
    ca::usize size() const noexcept { return len(); }

    /// @brief 判断列表是否为空。
    bool is_empty() const noexcept { return data_.empty(); }
    bool empty() const noexcept { return data_.empty(); }

    /// @brief 返回当前已分配容量。
    ca::usize capacity() const noexcept { return static_cast<ca::usize>(data_.capacity()); }

    /// @brief 返回底层连续存储指针；空列表时遵循 `std::vector::data()` 语义。
    const T* data() const noexcept { return data_.data(); }
    T*       data() noexcept { return data_.data(); }

    /// @brief 返回第一个元素指针；列表为空时返回 nullptr。
    const T* first() const noexcept
    {
        if (data_.empty())
            return nullptr;
        return &data_.front();
    }

    T* first() noexcept
    {
        if (data_.empty())
            return nullptr;
        return &data_.front();
    }

    /// @brief 返回最后一个元素指针；列表为空时返回 nullptr。
    const T* last() const noexcept
    {
        if (data_.empty())
            return nullptr;
        return &data_.back();
    }

    T* last() noexcept
    {
        if (data_.empty())
            return nullptr;
        return &data_.back();
    }

    /// @brief 复制 `[begin, end)` 范围并返回新的列表。
    /// @throws std::out_of_range 当范围不合法。
    ArrayList slice(ca::usize begin, ca::usize end) const
    {
        if (begin > end || end > len())
            throw std::out_of_range("ArrayList: slice range out of range");
        return from_slice(data_.data() + begin, end - begin);
    }

    ArrayList sub_list(ca::usize begin, ca::usize end) const { return slice(begin, end); }

    /// @brief 预留容量，不改变长度。
    void reserve(ca::usize capacity) { data_.reserve(capacity); }

    /// @brief 清空所有元素，保留已分配容量。
    void clear() noexcept { data_.clear(); }

    /// @brief 截断到指定长度；length 大于当前长度时不增长。
    void truncate(ca::usize length)
    {
        if (length < data_.size())
            data_.resize(length);
    }
    /// @brief 调整长度；增长部分做默认初始化。
    void resize(ca::usize length) { data_.resize(length); }

    /// @brief 调整长度；增长部分用 value 填充。
    void resize(ca::usize length, const T& value) { data_.resize(length, value); }

    /// @brief 追加一个元素到末尾。
    void add(const T& value) { data_.push_back(value); }
    void add(T&& value) { data_.push_back(std::move(value)); }
    void push(const T& value) { add(value); }
    void push(T&& value) { add(std::move(value)); }

    /// @brief 从连续数组追加元素。
    /// @throws std::invalid_argument 当 length 非 0 且 data 为空。
    void extend_from_slice(const T* data, ca::usize length)
    {
        if (length == 0)
            return;
        if (data == nullptr)
            throw std::invalid_argument("ArrayList: null slice data");
        data_.insert(data_.end(), data, data + length);
    }

    /// @brief 追加另一个列表的快照内容。
    void add_all(const ArrayList& other)
    {
        data_.insert(data_.end(), other.data_.begin(), other.data_.end());
    }

    /// @brief 在末尾原地构造元素并返回引用。
    template<typename... Args>
    T& emplace(Args&&... args)
    {
        data_.emplace_back(std::forward<Args>(args)...);
        return data_.back();
    }

    /// @brief 移除并返回最后一个元素；空列表返回 `std::nullopt`。
    std::optional<T> pop()
    {
        if (data_.empty())
            return std::nullopt;
        T value = std::move(data_.back());
        data_.pop_back();
        return value;
    }

    /// @brief 返回 index 位置元素引用。
    /// @throws std::out_of_range 当 index 越界。
    const T& get(ca::usize index) const
    {
        check_index(index);
        return data_[index];
    }

    T& get(ca::usize index)
    {
        check_index(index);
        return data_[index];
    }

    /// @brief 尝试获取 index 位置元素；越界返回 nullptr。
    const T* try_get(ca::usize index) const noexcept
    {
        if (index >= data_.size())
            return nullptr;
        return &data_[index];
    }

    T* try_get(ca::usize index) noexcept
    {
        if (index >= data_.size())
            return nullptr;
        return &data_[index];
    }

    const T& operator[](ca::usize index) const { return get(index); }
    T&       operator[](ca::usize index) { return get(index); }

    /// @brief 设置 index 位置元素。
    /// @throws std::out_of_range 当 index 越界。
    void set(ca::usize index, const T& value)
    {
        check_index(index);
        data_[index] = value;
    }

    void set(ca::usize index, T&& value)
    {
        check_index(index);
        data_[index] = std::move(value);
    }

    /// @brief 交换两个位置的元素。
    /// @throws std::out_of_range 当任一 index 越界。
    void swap(ca::usize lhs, ca::usize rhs)
    {
        check_index(lhs);
        check_index(rhs);
        using std::swap;
        swap(data_[lhs], data_[rhs]);
    }

    /// @brief 在 index 前插入元素，保持顺序。
    /// @throws std::out_of_range 当 index 大于当前长度。
    void insert(ca::usize index, const T& value)
    {
        check_insert_index(index);
        data_.insert(data_.begin() + static_cast<std::ptrdiff_t>(index), value);
    }

    void insert(ca::usize index, T&& value)
    {
        check_insert_index(index);
        data_.insert(data_.begin() + static_cast<std::ptrdiff_t>(index), std::move(value));
    }

    /// @brief 移除 index 位置元素并返回旧值，保持剩余元素顺序。
    /// @throws std::out_of_range 当 index 越界。
    T remove_at(ca::usize index)
    {
        check_index(index);
        auto it    = data_.begin() + static_cast<std::ptrdiff_t>(index);
        T    value = std::move(*it);
        data_.erase(it);
        return value;
    }

    /// @brief 移除 index 位置元素并返回旧值，不保证保持顺序。
    /// @throws std::out_of_range 当 index 越界。
    T swap_remove(ca::usize index)
    {
        check_index(index);
        T value = std::move(data_[index]);
        if (index + 1 < data_.size())
            data_[index] = std::move(data_.back());
        data_.pop_back();
        return value;
    }

    /// @brief 判断列表是否包含等于 value 的元素。
    bool contains(const T& value) const { return index_of(value) != npos; }

    /// @brief 返回第一个等于 value 的元素下标；未找到返回 npos。
    ca::usize index_of(const T& value) const
    {
        for (ca::usize i = 0; i < len(); ++i) {
            if (data_[i] == value)
                return i;
        }
        return npos;
    }

    /// @brief 返回最后一个等于 value 的元素下标；未找到返回 npos。
    ca::usize last_index_of(const T& value) const
    {
        for (ca::usize i = len(); i > 0; --i) {
            const ca::usize index = i - 1;
            if (data_[index] == value)
                return index;
        }
        return npos;
    }

    /// @brief 移除第一个等于 value 的元素。
    /// @return 找到并移除时返回 true。
    bool remove_value(const T& value)
    {
        const ca::usize index = index_of(value);
        if (index == npos)
            return false;

        remove_at(index);
        return true;
    }

    /// @brief 只保留谓词返回 true 的元素。
    template<typename Pred>
    void retain(Pred&& keep)
    {
        remove_if([&keep](const T& value) { return !keep(value); });
    }

    /// @brief 移除谓词返回 true 的元素。
    /// @return 被移除的元素数量。
    template<typename Pred>
    ca::usize remove_if(Pred&& remove)
    {
        const auto old_size = data_.size();
        auto       it = std::remove_if(data_.begin(), data_.end(), std::forward<Pred>(remove));
        data_.erase(it, data_.end());
        return static_cast<ca::usize>(old_size - data_.size());
    }

    iterator       begin() noexcept { return data_.begin(); }
    iterator       end() noexcept { return data_.end(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }
    const_iterator cend() const noexcept { return data_.cend(); }

private:
    void check_index(ca::usize index) const
    {
        if (index >= data_.size())
            throw std::out_of_range("ArrayList: index out of range");
    }

    void check_insert_index(ca::usize index) const
    {
        if (index > data_.size())
            throw std::out_of_range("ArrayList: insert index out of range");
    }

    std::vector<T> data_;
};

}   // namespace ca::collection
