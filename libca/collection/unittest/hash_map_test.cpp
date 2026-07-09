#include "libca/collection/hash_map.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ca::collection {

TEST(HashMapTest, PutGetContainsAndLen)
{
    auto map = HashMap<std::string, ca::i32>::with_capacity(8);

    EXPECT_TRUE(map.is_empty());
    EXPECT_FALSE(map.contains_key("one"));

    EXPECT_FALSE(map.put("one", 1).has_value());
    EXPECT_FALSE(map.put("two", 2).has_value());

    EXPECT_EQ(map.len(), 2u);
    EXPECT_TRUE(map.contains_key("one"));
    EXPECT_TRUE(map.contains_key("two"));
    EXPECT_TRUE(map.contains_value(1));
    EXPECT_TRUE(map.contains_value(2));
    EXPECT_FALSE(map.contains_value(3));
    ASSERT_NE(map.get("one"), nullptr);
    EXPECT_EQ(*map.get("one"), 1);
    EXPECT_EQ(map.get("missing"), nullptr);
}

TEST(HashMapTest, PutReturnsPreviousValueAndRemoveTakesOwnership)
{
    HashMap<std::string, ca::i32> map = {{"one", 1}, {"two", 2}};

    auto old = map.put("one", 11);

    ASSERT_TRUE(old.has_value());
    EXPECT_EQ(*old, 1);
    ASSERT_NE(map.get("one"), nullptr);
    EXPECT_EQ(*map.get("one"), 11);

    auto removed = map.remove("two");
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 2);
    EXPECT_FALSE(map.contains_key("two"));
    EXPECT_FALSE(map.remove("two").has_value());
}

TEST(HashMapTest, GetOrDefaultReturnsExistingValueOrDefault)
{
    HashMap<std::string, ca::i32> map = {{"one", 1}};

    EXPECT_EQ(map.get_or_default("one", 11), 1);
    EXPECT_EQ(map.get_or_default("two", 22), 22);
    EXPECT_FALSE(map.contains_key("two"));
    EXPECT_EQ(map.len(), 1u);
}

TEST(HashMapTest, FromEntriesAndPutAllOverwriteDuplicateKeys)
{
    const std::pair<std::string, ca::i32> entries[] = {
        {"one", 1},
        {"two", 2},
        {"one", 11},
    };

    auto map = HashMap<std::string, ca::i32>::from_entries(entries, 3);

    ASSERT_EQ(map.len(), 2u);
    EXPECT_EQ(*map.get("one"), 11);
    EXPECT_EQ(*map.get("two"), 2);

    const std::pair<std::string, ca::i32> more[] = {
        {"two", 22},
        {"three", 3},
    };
    map.put_all(more, 2);
    map.put_all(nullptr, 0);

    EXPECT_EQ(map.len(), 3u);
    EXPECT_EQ(*map.get("two"), 22);
    EXPECT_EQ(*map.get("three"), 3);

    auto other = HashMap<std::string, ca::i32>::from_entries(entries, 2);
    other.put("four", 4);
    map.put_all(other);

    EXPECT_EQ(map.len(), 4u);
    EXPECT_EQ(*map.get("one"), 1);
    EXPECT_EQ(*map.get("four"), 4);
    EXPECT_THROW(map.put_all(nullptr, 1), std::invalid_argument);
}

TEST(HashMapTest, IteratesOverEntries)
{
    HashMap<std::string, ca::i32> map = {{"one", 1}, {"two", 2}, {"three", 3}};

    ca::i32 sum = 0;
    for (const auto& item : map)
        sum += item.second;

    EXPECT_EQ(sum, 6);
}

TEST(HashMapTest, KeysAndValuesReturnSnapshots)
{
    HashMap<std::string, ca::i32> map = {{"one", 1}, {"two", 2}, {"three", 3}};

    auto keys = map.keys();
    auto values = map.values();
    map.put("four", 4);

    std::vector<std::string> key_values;
    for (const auto& key : keys)
        key_values.push_back(key);
    std::sort(key_values.begin(), key_values.end());

    std::vector<ca::i32> value_values;
    for (const auto& value : values)
        value_values.push_back(value);
    std::sort(value_values.begin(), value_values.end());

    EXPECT_EQ(key_values, (std::vector<std::string>{"one", "three", "two"}));
    EXPECT_EQ(value_values, (std::vector<ca::i32>{1, 2, 3}));
    EXPECT_EQ(keys.len(), 3u);
    EXPECT_EQ(values.len(), 3u);
    EXPECT_EQ(map.len(), 4u);
}

TEST(HashMapTest, EntriesReturnSnapshots)
{
    HashMap<std::string, ca::i32> map = {{"one", 1}, {"two", 2}, {"three", 3}};

    auto entries = map.entries();
    auto entry_list = map.entry_list();
    auto snake_entry_list = map.entry_list();
    map.put("four", 4);

    std::vector<std::pair<std::string, ca::i32>> entry_values;
    for (const auto& entry : entries)
        entry_values.push_back(entry);
    std::sort(entry_values.begin(), entry_values.end());

    std::vector<std::pair<std::string, ca::i32>> camel_values;
    for (const auto& entry : entry_list)
        camel_values.push_back(entry);
    std::sort(camel_values.begin(), camel_values.end());

    std::vector<std::pair<std::string, ca::i32>> snake_values;
    for (const auto& entry : snake_entry_list)
        snake_values.push_back(entry);
    std::sort(snake_values.begin(), snake_values.end());

    const std::vector<std::pair<std::string, ca::i32>> expected = {
        {"one", 1},
        {"three", 3},
        {"two", 2},
    };
    EXPECT_EQ(entry_values, expected);
    EXPECT_EQ(camel_values, expected);
    EXPECT_EQ(snake_values, expected);
    EXPECT_EQ(entries.len(), 3u);
    EXPECT_EQ(map.len(), 4u);
}

TEST(HashMapTest, RetainAndRemoveIf)
{
    HashMap<std::string, ca::i32> map = {
        {"one", 1},
        {"two", 2},
        {"three", 3},
        {"four", 4},
    };

    EXPECT_EQ(map.remove_if([](const std::string&, ca::i32 value) {
        return value % 2 == 0;
    }), 2u);
    EXPECT_EQ(map.len(), 2u);
    EXPECT_TRUE(map.contains_key("one"));
    EXPECT_TRUE(map.contains_key("three"));
    EXPECT_FALSE(map.contains_key("two"));

    map.retain([](const std::string& key, ca::i32) {
        return key.size() > 3;
    });
    EXPECT_EQ(map.len(), 1u);
    EXPECT_FALSE(map.contains_key("one"));
    EXPECT_TRUE(map.contains_key("three"));

    EXPECT_EQ(map.remove_if([](const std::string&, ca::i32 value) {
        return value > 10;
    }), 0u);
    EXPECT_EQ(map.len(), 1u);
}

TEST(HashMapTest, SupportsMoveOnlyValues)
{
    HashMap<std::string, std::unique_ptr<ca::i32>> map;

    EXPECT_FALSE(map.put(std::string("one"), std::make_unique<ca::i32>(1)).has_value());
    auto old = map.put(std::string("one"), std::make_unique<ca::i32>(11));

    ASSERT_TRUE(old.has_value());
    ASSERT_NE(*old, nullptr);
    EXPECT_EQ(**old, 1);

    auto* current = map.get("one");
    ASSERT_NE(current, nullptr);
    ASSERT_NE(*current, nullptr);
    EXPECT_EQ(**current, 11);

    auto removed = map.remove("one");
    ASSERT_TRUE(removed.has_value());
    ASSERT_NE(*removed, nullptr);
    EXPECT_EQ(**removed, 11);
    EXPECT_TRUE(map.is_empty());
}

TEST(HashMapTest, RemoveIfSupportsMoveOnlyValues)
{
    HashMap<std::string, std::unique_ptr<ca::i32>> map;
    map.put(std::string("one"), std::make_unique<ca::i32>(1));
    map.put(std::string("two"), std::make_unique<ca::i32>(2));
    map.put(std::string("three"), std::make_unique<ca::i32>(3));

    EXPECT_EQ(map.remove_if([](const std::string&, const std::unique_ptr<ca::i32>& value) {
        return value != nullptr && *value == 2;
    }), 1u);

    EXPECT_EQ(map.len(), 2u);
    EXPECT_TRUE(map.contains_key("one"));
    EXPECT_FALSE(map.contains_key("two"));
    EXPECT_TRUE(map.contains_key("three"));
}

TEST(HashMapTest, GetOrInsertReturnsExistingValue)
{
    HashMap<std::string, ca::i32> map = {{"one", 1}};

    ca::i32& value = map.get_or_insert("one", 11);

    EXPECT_EQ(value, 1);
    ASSERT_NE(map.get("one"), nullptr);
    EXPECT_EQ(*map.get("one"), 1);

    value = 2;
    EXPECT_EQ(*map.get("one"), 2);
}

TEST(HashMapTest, GetOrInsertAddsMissingValue)
{
    HashMap<std::string, ca::i32> map;

    ca::i32& camel = map.get_or_insert("one", 1);
    ca::i32& snake = map.get_or_insert(std::string("two"), 2);

    EXPECT_EQ(camel, 1);
    EXPECT_EQ(snake, 2);
    EXPECT_EQ(map.len(), 2u);
    EXPECT_EQ(*map.get("one"), 1);
    EXPECT_EQ(*map.get("two"), 2);
}

TEST(HashMapTest, GetOrInsertWithBuildsOnlyWhenMissing)
{
    HashMap<std::string, ca::i32> map = {{"one", 1}};
    ca::i32 calls = 0;

    ca::i32& existing = map.get_or_insert_with("one", [&calls]() {
        ++calls;
        return 11;
    });
    ca::i32& missing = map.get_or_insert_with(std::string("two"), [&calls]() {
        ++calls;
        return 2;
    });

    EXPECT_EQ(existing, 1);
    EXPECT_EQ(missing, 2);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(map.len(), 2u);
}

TEST(HashMapTest, GetOrInsertSupportsMoveOnlyValues)
{
    HashMap<std::string, std::unique_ptr<ca::i32>> map;

    auto& inserted = map.get_or_insert(std::string("one"), std::make_unique<ca::i32>(1));
    ASSERT_NE(inserted, nullptr);
    EXPECT_EQ(*inserted, 1);

    auto& existing = map.get_or_insert_with(std::string("one"), []() {
        return std::make_unique<ca::i32>(11);
    });
    ASSERT_NE(existing, nullptr);
    EXPECT_EQ(*existing, 1);

    auto& built = map.get_or_insert_with(std::string("two"), []() {
        return std::make_unique<ca::i32>(2);
    });
    ASSERT_NE(built, nullptr);
    EXPECT_EQ(*built, 2);
}

}  // namespace ca::collection
