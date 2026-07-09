#pragma once

#include "array_list.hpp"
#include "libca/core/datatype.hpp"

#include <functional>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <utility>

/// @file hash_set.hpp
/// @brief HashSet — Rust-like 基础 API 的哈希集合容器。

namespace ca::collection {

/// @brief 基于 `std::unordered_set` 的拥有型哈希集合容器。
///
/// `HashSet` 提供 Rust-like 的 snake_case API。集合不保证迭代顺序，查询类 API
/// 使用 bool 或只读指针表达结果，`take()` / `replace()` 用 `std::optional<T>`
/// 返回被移出的旧值所有权。
template<typename T, typename Hash = std::hash<T>, typename KeyEqual = std::equal_to<T>>
class HashSet {
public:
    using value_type = T;
    using iterator = typename std::unordered_set<T, Hash, KeyEqual>::iterator;
    using const_iterator = typename std::unordered_set<T, Hash, KeyEqual>::const_iterator;

    HashSet() = default;

    /// @brief 用初始化列表构造集合，重复值会被去重。
    HashSet(std::initializer_list<T> values)
    {
        reserve(static_cast<ca::usize>(values.size()));
        for (const auto& value : values)
            add(value);
    }

    /// @brief 创建并预留 capacity 桶容量的空集合。
    static HashSet with_capacity(ca::usize capacity)
    {
        HashSet out;
        out.reserve(capacity);
        return out;
    }

    /// @brief 从连续数组拷贝构造集合。
    /// @throws std::invalid_argument 当 length 非 0 且 data 为空。
    static HashSet from_slice(const T* data, ca::usize length)
    {
        HashSet out;
        out.extend_from_slice(data, length);
        return out;
    }

    /// @brief 返回集合元素数量。
    ca::usize len() const noexcept { return static_cast<ca::usize>(data_.size()); }
    ca::usize size() const noexcept { return len(); }

    /// @brief 判断集合是否为空。
    bool is_empty() const noexcept { return data_.empty(); }
    bool empty() const noexcept { return data_.empty(); }

    /// @brief 预留底层哈希表容量，不改变元素数量。
    void reserve(ca::usize capacity) { data_.reserve(capacity); }

    /// @brief 清空所有元素。
    void clear() noexcept { data_.clear(); }

    /// @brief 判断集合是否包含等价 value。
    bool contains(const T& value) const { return data_.find(value) != data_.end(); }

    /// @brief 返回集合中等价 value 的只读指针；不存在时返回 nullptr。
    const T* get(const T& value) const
    {
        auto it = data_.find(value);
        if (it == data_.end())
            return nullptr;
        return &*it;
    }

    /// @brief 判断当前集合是否是 other 的子集。
    bool is_subset_of(const HashSet& other) const
    {
        if (len() > other.len())
            return false;
        for (const auto& value : data_) {
            if (!other.contains(value))
                return false;
        }
        return true;
    }

    /// @brief 判断当前集合是否是 other 的超集。
    bool is_superset_of(const HashSet& other) const { return other.is_subset_of(*this); }

    /// @brief 判断当前集合与 other 是否没有交集。
    bool is_disjoint(const HashSet& other) const
    {
        const HashSet* smaller = this;
        const HashSet* larger = &other;
        if (other.len() < len()) {
            smaller = &other;
            larger = this;
        }

        for (const auto& value : smaller->data_) {
            if (larger->contains(value))
                return false;
        }
        return true;
    }

    /// @brief 返回并集快照。
    HashSet union_with(const HashSet& other) const
    {
        auto out = HashSet::with_capacity(len() + other.len());
        out.add_all(*this);
        out.add_all(other);
        return out;
    }

    /// @brief 返回交集快照。
    HashSet intersection_with(const HashSet& other) const
    {
        const HashSet* smaller = this;
        const HashSet* larger = &other;
        if (other.len() < len()) {
            smaller = &other;
            larger = this;
        }

        auto out = HashSet::with_capacity(smaller->len());
        for (const auto& value : smaller->data_) {
            if (larger->contains(value))
                out.add(value);
        }
        return out;
    }

    /// @brief 返回差集快照，即当前集合中存在而 other 中不存在的元素。
    HashSet difference_with(const HashSet& other) const
    {
        auto out = HashSet::with_capacity(len());
        for (const auto& value : data_) {
            if (!other.contains(value))
                out.add(value);
        }
        return out;
    }

    /// @brief 返回当前元素的快照列表。
    ArrayList<T> values() const
    {
        auto out = ArrayList<T>::with_capacity(len());
        for (const auto& value : data_)
            out.add(value);
        return out;
    }

    ArrayList<T> to_array_list() const { return values(); }

    /// @brief 插入元素。
    /// @return 新插入时返回 true；等价元素已存在时返回 false。
    bool add(const T& value) { return data_.insert(value).second; }
    bool add(T&& value) { return data_.insert(std::move(value)).second; }
    bool insert(const T& value) { return add(value); }
    bool insert(T&& value) { return add(std::move(value)); }

    /// @brief 从连续数组批量插入元素。
    /// @throws std::invalid_argument 当 length 非 0 且 data 为空。
    void extend_from_slice(const T* data, ca::usize length)
    {
        if (length == 0)
            return;
        if (data == nullptr)
            throw std::invalid_argument("HashSet: null slice data");
        data_.insert(data, data + length);
    }

    /// @brief 批量插入另一个集合中的元素。
    void add_all(const HashSet& other) { data_.insert(other.data_.begin(), other.data_.end()); }

    /// @brief 移除等价 value。
    /// @return 找到并移除时返回 true。
    bool remove(const T& value) { return data_.erase(value) != 0; }

    /// @brief 移除并返回集合中等价 value 的实际存储值。
    /// @return value 不存在时返回 `std::nullopt`。
    std::optional<T> take(const T& value)
    {
        auto node = data_.extract(value);
        if (node.empty())
            return std::nullopt;
        return std::move(node.value());
    }

    /// @brief 插入 value；如果等价旧值已存在，则替换并返回旧值。
    /// @return 新插入时返回 `std::nullopt`，替换时返回被移出的旧值。
    std::optional<T> replace(T value)
    {
        auto node = data_.extract(value);
        if (node.empty()) {
            data_.insert(std::move(value));
            return std::nullopt;
        }

        T old = std::move(node.value());
        data_.insert(std::move(value));
        return std::optional<T>(std::move(old));
    }

    /// @brief 只保留谓词返回 true 的元素。
    template<typename Pred>
    void retain(Pred&& keep)
    {
        remove_if([&keep](const T& value) {
            return !keep(value);
        });
    }

    /// @brief 移除谓词返回 true 的元素。
    /// @return 被移除的元素数量。
    template<typename Pred>
    ca::usize remove_if(Pred&& remove)
    {
        ca::usize removed = 0;
        for (auto it = data_.begin(); it != data_.end();) {
            if (remove(*it)) {
                it = data_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    iterator begin() noexcept { return data_.begin(); }
    iterator end() noexcept { return data_.end(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }
    const_iterator cend() const noexcept { return data_.cend(); }

private:
    std::unordered_set<T, Hash, KeyEqual> data_;
};

}  // namespace ca::collection
