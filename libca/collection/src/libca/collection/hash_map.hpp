#pragma once

#include "array_list.hpp"
#include "libca/core/datatype.hpp"

#include <functional>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

/// @file hash_map.hpp
/// @brief HashMap — Rust-like 基础 API 的哈希映射容器。

namespace ca::collection {

/// @brief 基于 `std::unordered_map` 的拥有型哈希映射容器。
///
/// `HashMap` 提供 Rust-like 的 snake_case API，并用 `ArrayList` 返回 key/value/
/// entry 快照。查询类 API 使用指针表达“存在或不存在”，移除和覆盖类 API 使用
/// `std::optional<V>` 返回旧值所有权。迭代顺序遵循底层哈希表，不保证稳定。
template<typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class HashMap
{
public:
    using key_type       = K;
    using mapped_type    = V;
    using value_type     = typename std::unordered_map<K, V, Hash, KeyEqual>::value_type;
    using iterator       = typename std::unordered_map<K, V, Hash, KeyEqual>::iterator;
    using const_iterator = typename std::unordered_map<K, V, Hash, KeyEqual>::const_iterator;

    HashMap() = default;

    /// @brief 用初始化列表构造映射；重复 key 时后面的值覆盖前面的值。
    HashMap(std::initializer_list<std::pair<K, V>> values)
    {
        reserve(static_cast<ca::usize>(values.size()));
        for (const auto& item : values)
            put(item.first, item.second);
    }

    /// @brief 创建并预留 capacity 桶容量的空映射。
    static HashMap with_capacity(ca::usize capacity)
    {
        HashMap out;
        out.reserve(capacity);
        return out;
    }

    /// @brief 从连续 entry 数组构造映射。
    /// @throws std::invalid_argument 当 length 非 0 且 entries 为空。
    static HashMap from_entries(const std::pair<K, V>* entries, ca::usize length)
    {
        HashMap out;
        out.put_all(entries, length);
        return out;
    }

    /// @brief 返回键值对数量。
    ca::usize len() const noexcept { return static_cast<ca::usize>(data_.size()); }
    ca::usize size() const noexcept { return len(); }

    /// @brief 判断映射是否为空。
    bool is_empty() const noexcept { return data_.empty(); }
    bool empty() const noexcept { return data_.empty(); }

    /// @brief 预留底层哈希表容量，不改变元素数量。
    void reserve(ca::usize capacity) { data_.reserve(capacity); }

    /// @brief 清空所有键值对。
    void clear() noexcept { data_.clear(); }

    /// @brief 判断 key 是否存在。
    bool contains_key(const K& key) const { return data_.find(key) != data_.end(); }

    /// @brief 线性扫描判断是否存在等于 value 的值。
    bool contains_value(const V& value) const
    {
        for (const auto& item : data_) {
            if (item.second == value)
                return true;
        }
        return false;
    }

    /// @brief 获取 key 对应值的可变指针；不存在时返回 nullptr。
    V* get(const K& key)
    {
        auto it = data_.find(key);
        if (it == data_.end())
            return nullptr;
        return &it->second;
    }

    /// @brief 获取 key 对应值的只读指针；不存在时返回 nullptr。
    const V* get(const K& key) const
    {
        auto it = data_.find(key);
        if (it == data_.end())
            return nullptr;
        return &it->second;
    }

    /// @brief 返回 key 对应值的拷贝；不存在时返回 default_value。
    V get_or_default(const K& key, const V& default_value) const
    {
        auto it = data_.find(key);
        if (it == data_.end())
            return default_value;
        return it->second;
    }

    /// @brief 返回当前所有 key 的快照列表。
    ArrayList<K> keys() const
    {
        auto out = ArrayList<K>::with_capacity(len());
        for (const auto& item : data_)
            out.add(item.first);
        return out;
    }

    /// @brief 返回当前所有 value 的快照列表。
    ArrayList<V> values() const
    {
        auto out = ArrayList<V>::with_capacity(len());
        for (const auto& item : data_)
            out.add(item.second);
        return out;
    }

    /// @brief 返回当前所有键值对的快照列表。
    ArrayList<std::pair<K, V>> entries() const
    {
        auto out = ArrayList<std::pair<K, V>>::with_capacity(len());
        for (const auto& item : data_)
            out.add(std::pair<K, V>(item.first, item.second));
        return out;
    }

    ArrayList<std::pair<K, V>> entry_list() const { return entries(); }

    /// @brief 获取 key 对应值；不存在时插入 value 并返回新值引用。
    V& get_or_insert(const K& key, const V& value)
    {
        auto [it, _] = data_.try_emplace(key, value);
        return it->second;
    }

    V& get_or_insert(K&& key, V&& value)
    {
        auto [it, _] = data_.try_emplace(std::move(key), std::move(value));
        return it->second;
    }

    /// @brief 获取 key 对应值；不存在时惰性调用 make_value 构造新值。
    template<typename F>
    V& get_or_insert_with(const K& key, F&& make_value)
    {
        auto it = data_.find(key);
        if (it != data_.end())
            return it->second;

        auto [inserted, _] = data_.emplace(key, std::forward<F>(make_value)());
        return inserted->second;
    }

    template<typename F>
    V& get_or_insert_with(K&& key, F&& make_value)
    {
        auto it = data_.find(key);
        if (it != data_.end())
            return it->second;

        auto [inserted, _] = data_.emplace(std::move(key), std::forward<F>(make_value)());
        return inserted->second;
    }

    /// @brief 插入或覆盖键值对。
    /// @return 覆盖已有 key 时返回旧值；新插入时返回 `std::nullopt`。
    std::optional<V> put(const K& key, const V& value)
    {
        auto it = data_.find(key);
        if (it == data_.end()) {
            data_.emplace(key, value);
            return std::nullopt;
        }

        V old      = std::move(it->second);
        it->second = value;
        return old;
    }

    std::optional<V> put(K&& key, V&& value)
    {
        auto it = data_.find(key);
        if (it == data_.end()) {
            data_.emplace(std::move(key), std::move(value));
            return std::nullopt;
        }

        V old      = std::move(it->second);
        it->second = std::move(value);
        return old;
    }

    /// @brief 批量插入或覆盖 entry 数组中的键值对。
    /// @throws std::invalid_argument 当 length 非 0 且 entries 为空。
    void put_all(const std::pair<K, V>* entries, ca::usize length)
    {
        if (length == 0)
            return;
        if (entries == nullptr)
            throw std::invalid_argument("HashMap: null entries data");

        reserve(len() + length);
        for (ca::usize i = 0; i < length; ++i)
            put(entries[i].first, entries[i].second);
    }

    /// @brief 批量插入或覆盖另一个映射中的键值对。
    void put_all(const HashMap& other)
    {
        reserve(len() + other.len());
        for (const auto& item : other.data_)
            put(item.first, item.second);
    }

    /// @brief 移除 key 对应键值对并返回旧值。
    /// @return key 不存在时返回 `std::nullopt`。
    std::optional<V> remove(const K& key)
    {
        auto it = data_.find(key);
        if (it == data_.end())
            return std::nullopt;

        V old = std::move(it->second);
        data_.erase(it);
        return old;
    }

    /// @brief 只保留谓词返回 true 的键值对。
    template<typename Pred>
    void retain(Pred&& keep)
    {
        remove_if([&keep](const K& key, const V& value) { return !keep(key, value); });
    }

    /// @brief 移除谓词返回 true 的键值对。
    /// @return 被移除的键值对数量。
    template<typename Pred>
    ca::usize remove_if(Pred&& remove)
    {
        ca::usize removed = 0;
        for (auto it = data_.begin(); it != data_.end();) {
            if (remove(it->first, it->second)) {
                it = data_.erase(it);
                ++removed;
            }
            else {
                ++it;
            }
        }
        return removed;
    }

    iterator       begin() noexcept { return data_.begin(); }
    iterator       end() noexcept { return data_.end(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }
    const_iterator cend() const noexcept { return data_.cend(); }

private:
    std::unordered_map<K, V, Hash, KeyEqual> data_;
};

}   // namespace ca::collection
