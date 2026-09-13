#pragma once

#include "libca/core/datatype.hpp"

#include <functional>
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

/// @file lru_cache.hpp
/// @brief LruCache — 容量受限、按 LRU 策略淘汰的哈希缓存。

namespace ca::collection {

/// @brief 缓存命中统计：命中、未命中与淘汰的累计次数。
struct CacheStatus {
    /// get 命中次数。
    ca::usize hits{0};
    /// get 未命中次数。
    ca::usize misses{0};
    /// 因容量不足被淘汰的条目数。
    ca::usize evictions{0};
};

/// @brief 容量受限的 LRU 缓存。
///
/// `LruCache` 用双向链表 + 哈希索引实现，`get`/`put` 均摊 O(1)。链表头部是
/// 最近使用的条目，尾部是最久未用的条目；插入超过容量时从尾部淘汰。
/// `get` 命中会把条目提升为最新；`put` 已有 key 视为一次使用并覆盖值。
///
/// 所有者语义：缓存拥有全部 value；`remove`/`clear` 与淘汰都是销毁式所有权
/// 转移，`on_evict` 回调在条目销毁前收到 `const K&`/`const V&` 只读快照。
///
/// @note 纯数据结构，内部不加锁；多线程使用需外层同步。
/// @throws std::invalid_argument 构造时 capacity 为 0。
template<typename K, typename V, typename Hash = std::hash<K>>
class LruCache {
public:
    /// @brief 淘汰回调签名：参数为被淘汰条目的 key 与 value 只读引用。
    using EvictCallback = std::function<void(const K&, const V&)>;

    /// @brief 以最大容量构造空缓存。
    /// @throws std::invalid_argument 当 capacity 为 0。
    explicit LruCache(ca::usize capacity) : LruCache(capacity, nullptr) {}

    /// @brief 以最大容量构造空缓存并注册淘汰回调。
    /// @param on_evict 因容量不足被淘汰时触发；`remove`/`clear` 不触发。
    /// @throws std::invalid_argument 当 capacity 为 0。
    LruCache(ca::usize capacity, EvictCallback on_evict) : capacity_(capacity), on_evict_(std::move(on_evict))
    {
        if (capacity_ == 0)
            throw std::invalid_argument("LruCache: capacity must be greater than 0");
    }

    /// @brief 返回缓存可容纳的最大条目数。
    ca::usize capacity() const noexcept { return capacity_; }

    /// @brief 返回当前条目数。
    ca::usize len() const noexcept { return static_cast<ca::usize>(items_.size()); }

    /// @brief 判断缓存是否为空。
    bool is_empty() const noexcept { return items_.empty(); }

    /// @brief 判断 key 是否存在；不提升热度，也不影响命中统计。
    bool contains_key(const K& key) const { return index_.find(key) != index_.end(); }

    /// @brief 读取 key 对应值；命中时提升为最近使用并计入 hits。
    /// @return 命中返回值指针（指向缓存内部存储），未命中计入 misses 并返回 nullptr。
    V* get(const K& key)
    {
        auto it = index_.find(key);
        if (it == index_.end()) {
            status_.misses += 1;
            return nullptr;
        }
        touch(it->second);
        status_.hits += 1;
        return &it->second->second;
    }

    /// @brief 插入或覆盖 key；已有 key 覆盖值并提升热度，新 key 超容量时淘汰最久未用条目。
    /// @note 淘汰时先调用 on_evict（如已注册）再销毁条目，并计入 evictions。
    void put(const K& key, V value)
    {
        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->second = std::move(value);
            touch(it->second);
            return;
        }

        if (items_.size() >= capacity_)
            evict_lru();

        items_.emplace_front(key, std::move(value));
        index_[key] = items_.begin();
    }

    /// @brief 移除 key 对应条目并返回旧值。
    /// @return key 不存在返回 `std::nullopt`；显式移除不计入淘汰统计，也不触发 on_evict。
    std::optional<V> remove(const K& key)
    {
        auto it = index_.find(key);
        if (it == index_.end())
            return std::nullopt;

        V old = std::move(it->second->second);
        items_.erase(it->second);
        index_.erase(it);
        return old;
    }

    /// @brief 清空所有条目；不计入淘汰统计，也不触发 on_evict，命中统计保留。
    void clear() noexcept
    {
        items_.clear();
        index_.clear();
    }

    /// @brief 返回命中统计的只读引用。
    const CacheStatus& status() const noexcept { return status_; }

private:
    using ListIterator = typename std::list<std::pair<K, V>>::iterator;

    /// @brief 把条目移动到链表头部，标记为最近使用。
    void touch(ListIterator node)
    {
        items_.splice(items_.begin(), items_, node);
    }

    /// @brief 淘汰链表尾部最久未用条目：先回调后销毁，并计入 evictions。
    void evict_lru()
    {
        auto& lru = items_.back();
        if (on_evict_)
            on_evict_(lru.first, lru.second);
        index_.erase(lru.first);
        items_.pop_back();
        status_.evictions += 1;
    }

    ca::usize capacity_;
    // 头部为最近使用，尾部为最久未用；节点地址稳定，迭代器可长期保存在 index_ 中。
    std::list<std::pair<K, V>> items_;
    std::unordered_map<K, ListIterator, Hash> index_;
    CacheStatus status_;
    EvictCallback on_evict_;
};

}  // namespace ca::collection
