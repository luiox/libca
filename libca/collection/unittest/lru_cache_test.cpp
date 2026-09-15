#include <gtest/gtest.h>

#include "libca/collection/lru_cache.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ca::collection {

// 基础 put/get/len/is_empty/contains_key。
TEST(LruCacheTest, PutGetContainsAndLen)
{
    LruCache<std::string, ca::i32> cache(4);

    EXPECT_TRUE(cache.is_empty());
    EXPECT_EQ(cache.len(), 0u);
    EXPECT_EQ(cache.capacity(), 4u);

    cache.put("a", 1);
    cache.put("b", 2);
    EXPECT_EQ(cache.len(), 2u);
    EXPECT_FALSE(cache.is_empty());

    ASSERT_NE(cache.get("a"), nullptr);
    EXPECT_EQ(*cache.get("a"), 1);
    EXPECT_EQ(cache.get("missing"), nullptr);

    EXPECT_TRUE(cache.contains_key("a"));
    EXPECT_FALSE(cache.contains_key("missing"));
}

// get 命中提升热度：被访问的条目不淘汰，最久未访问的先淘汰。
TEST(LruCacheTest, GetPromotesRecency)
{
    LruCache<std::string, ca::i32> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);

    // 访问 a，使 b 成为最久未用
    ASSERT_NE(cache.get("a"), nullptr);
    EXPECT_EQ(*cache.get("a"), 1);

    cache.put("c", 3);
    EXPECT_FALSE(cache.contains_key("b"));
    EXPECT_TRUE(cache.contains_key("a"));
    EXPECT_TRUE(cache.contains_key("c"));
    EXPECT_EQ(cache.status().evictions, 1u);
}

// put 已有 key：覆盖值并提升热度，不触发淘汰。
TEST(LruCacheTest, OverwriteUpdatesValueAndPromotes)
{
    LruCache<std::string, ca::i32> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);

    cache.put("a", 11);
    EXPECT_EQ(cache.len(), 2u);
    EXPECT_EQ(cache.status().evictions, 0u);
    ASSERT_NE(cache.get("a"), nullptr);
    EXPECT_EQ(*cache.get("a"), 11);

    // a 刚被提升，淘汰的应是 b
    cache.put("c", 3);
    EXPECT_TRUE(cache.contains_key("a"));
    EXPECT_FALSE(cache.contains_key("b"));
}

// 淘汰顺序为最久未用优先，on_evict 依次收到被淘汰的 key/value。
TEST(LruCacheTest, EvictionOrderAndCallbackArguments)
{
    std::vector<std::pair<std::string, ca::i32>> evicted;
    LruCache<std::string, ca::i32> cache(2, [&evicted](const std::string& key, const ca::i32& value) {
        evicted.emplace_back(key, value);
    });

    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    cache.put("d", 4);

    ASSERT_EQ(evicted.size(), 2u);
    EXPECT_EQ(evicted[0], std::make_pair(std::string("a"), 1));
    EXPECT_EQ(evicted[1], std::make_pair(std::string("b"), 2));

    EXPECT_EQ(cache.status().evictions, 2u);
    EXPECT_EQ(cache.len(), 2u);
    EXPECT_TRUE(cache.contains_key("c"));
    EXPECT_TRUE(cache.contains_key("d"));
}

// hits/misses/evictions 精确计数；contains_key 不影响统计。
TEST(LruCacheTest, StatusCounters)
{
    LruCache<std::string, ca::i32> cache(1);

    EXPECT_FALSE(cache.contains_key("x"));
    EXPECT_EQ(cache.get("x"), nullptr);
    EXPECT_EQ(cache.status().misses, 1u);
    EXPECT_EQ(cache.status().hits, 0u);

    cache.put("x", 1);
    EXPECT_NE(cache.get("x"), nullptr);
    EXPECT_EQ(cache.status().hits, 1u);

    cache.put("y", 2);
    EXPECT_EQ(cache.status().evictions, 1u);
}

// remove 返回旧值；显式移除不计淘汰、不触发 on_evict。
TEST(LruCacheTest, RemoveDoesNotCountAsEviction)
{
    std::vector<std::string> evicted_keys;
    LruCache<std::string, ca::i32> cache(2, [&evicted_keys](const std::string& key, const ca::i32&) {
        evicted_keys.push_back(key);
    });
    cache.put("a", 1);
    cache.put("b", 2);

    auto removed = cache.remove("a");
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 1);
    EXPECT_FALSE(cache.remove("a").has_value());
    EXPECT_EQ(cache.len(), 1u);

    EXPECT_TRUE(evicted_keys.empty());
    EXPECT_EQ(cache.status().evictions, 0u);
}

// clear 清空条目但保留统计，也不触发 on_evict。
TEST(LruCacheTest, ClearKeepsStatus)
{
    LruCache<std::string, ca::i32> cache(2);
    cache.put("a", 1);
    cache.get("a");
    cache.put("b", 2);
    cache.put("c", 3);
    ASSERT_EQ(cache.status().evictions, 1u);

    cache.clear();
    EXPECT_TRUE(cache.is_empty());
    EXPECT_EQ(cache.get("a"), nullptr);

    // clear 本身不算淘汰；历史统计保留，miss 正常累计
    EXPECT_EQ(cache.status().evictions, 1u);
    EXPECT_EQ(cache.status().hits, 1u);
    EXPECT_EQ(cache.status().misses, 1u);
}

// 容量为 1 的边界：任何新 key 插入都会淘汰旧条目。
TEST(LruCacheTest, CapacityOneBoundary)
{
    std::vector<std::string> evicted_keys;
    LruCache<std::string, ca::i32> cache(1, [&evicted_keys](const std::string& key, const ca::i32&) {
        evicted_keys.push_back(key);
    });

    cache.put("a", 1);
    EXPECT_EQ(cache.len(), 1u);
    ASSERT_NE(cache.get("a"), nullptr);

    cache.put("b", 2);
    EXPECT_EQ(cache.len(), 1u);
    EXPECT_FALSE(cache.contains_key("a"));
    EXPECT_EQ(evicted_keys, std::vector<std::string>{"a"});
    ASSERT_NE(cache.get("b"), nullptr);
    EXPECT_EQ(*cache.get("b"), 2);
    EXPECT_EQ(cache.status().evictions, 1u);
}

// 容量为 0 无法存放任何条目，构造期直接报错。
TEST(LruCacheTest, CapacityZeroThrows)
{
    using StringCache = LruCache<std::string, ca::i32>;

    EXPECT_THROW((StringCache(0)), std::invalid_argument);
    EXPECT_THROW((StringCache(0, nullptr)), std::invalid_argument);
}

}  // namespace ca::collection
