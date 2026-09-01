#include "libca/collection/hash_set.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ca::collection {

namespace {

struct NamedId
{
    ca::i32     id;
    std::string name;
};

struct NamedIdHash
{
    std::size_t operator()(const NamedId& value) const { return std::hash<ca::i32>{}(value.id); }
};

struct NamedIdEqual
{
    bool operator()(const NamedId& left, const NamedId& right) const { return left.id == right.id; }
};

}   // namespace

TEST(HashSetTest, AddContainsAndLen)
{
    auto set = HashSet<std::string>::with_capacity(8);

    EXPECT_TRUE(set.is_empty());
    EXPECT_FALSE(set.contains("one"));

    EXPECT_TRUE(set.add("one"));
    EXPECT_TRUE(set.insert(std::string("two")));
    EXPECT_FALSE(set.add("one"));

    EXPECT_EQ(set.len(), 2u);
    EXPECT_TRUE(set.contains("one"));
    EXPECT_TRUE(set.contains("two"));
    EXPECT_FALSE(set.contains("missing"));
    ASSERT_NE(set.get("one"), nullptr);
    EXPECT_EQ(*set.get("one"), "one");
    EXPECT_EQ(set.get("missing"), nullptr);
}

TEST(HashSetTest, RemoveAndClear)
{
    HashSet<std::string> set = {"one", "two", "two"};

    EXPECT_EQ(set.len(), 2u);
    EXPECT_TRUE(set.remove("two"));
    EXPECT_FALSE(set.contains("two"));
    EXPECT_FALSE(set.remove("two"));

    set.clear();
    EXPECT_TRUE(set.is_empty());
}

TEST(HashSetTest, TakeRemovesAndReturnsOwnedValue)
{
    HashSet<std::string> set = {"one", "two"};

    auto taken = set.take("two");

    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(*taken, "two");
    EXPECT_EQ(set.len(), 1u);
    EXPECT_FALSE(set.contains("two"));
    EXPECT_TRUE(set.contains("one"));
    EXPECT_FALSE(set.take("missing").has_value());
    EXPECT_EQ(set.len(), 1u);
}

TEST(HashSetTest, ReplaceReturnsOldEquivalentValue)
{
    HashSet<NamedId, NamedIdHash, NamedIdEqual> set;

    auto inserted = set.replace(NamedId{1, "old"});

    EXPECT_FALSE(inserted.has_value());
    EXPECT_EQ(set.len(), 1u);

    auto replaced = set.replace(NamedId{1, "new"});

    ASSERT_TRUE(replaced.has_value());
    EXPECT_EQ(replaced->id, 1);
    EXPECT_EQ(replaced->name, "old");
    EXPECT_EQ(set.len(), 1u);

    const auto* current = set.get(NamedId{1, "probe"});
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->name, "new");

    auto second_insert = set.replace(NamedId{2, "two"});

    EXPECT_FALSE(second_insert.has_value());
    EXPECT_EQ(set.len(), 2u);
    ASSERT_NE(set.get(NamedId{2, "probe"}), nullptr);
    EXPECT_EQ(set.get(NamedId{2, "probe"})->name, "two");
}

TEST(HashSetTest, FromSliceAndBulkAddDeduplicates)
{
    const ca::i32 raw[] = {1, 2, 2, 3};
    auto          set   = HashSet<ca::i32>::from_slice(raw, 4);

    EXPECT_EQ(set.len(), 3u);
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(2));
    EXPECT_TRUE(set.contains(3));

    const ca::i32 more[] = {3, 4, 5, 5};
    set.extend_from_slice(more, 4);
    set.extend_from_slice(nullptr, 0);

    EXPECT_EQ(set.len(), 5u);
    EXPECT_TRUE(set.contains(4));
    EXPECT_TRUE(set.contains(5));

    auto other = HashSet<ca::i32>::from_slice(raw, 2);
    other.add(9);
    set.add_all(other);
    set.add_all(HashSet<ca::i32>{9, 10});

    EXPECT_EQ(set.len(), 7u);
    EXPECT_TRUE(set.contains(9));
    EXPECT_TRUE(set.contains(10));
    EXPECT_THROW(set.extend_from_slice(nullptr, 1), std::invalid_argument);
}

TEST(HashSetTest, IteratesOverValues)
{
    HashSet<ca::i32> set = {1, 2, 3};

    ca::i32 sum = 0;
    for (const auto& value : set)
        sum += value;

    EXPECT_EQ(sum, 6);
}

TEST(HashSetTest, ValuesAndToArrayListReturnSnapshots)
{
    HashSet<std::string> set = {"one", "two", "three"};

    auto values     = set.values();
    auto array_list = set.to_array_list();
    auto snake_list = set.to_array_list();
    set.add("four");

    std::vector<std::string> value_items;
    for (const auto& value : values)
        value_items.push_back(value);
    std::sort(value_items.begin(), value_items.end());

    std::vector<std::string> array_items;
    for (const auto& value : array_list)
        array_items.push_back(value);
    std::sort(array_items.begin(), array_items.end());

    std::vector<std::string> snake_items;
    for (const auto& value : snake_list)
        snake_items.push_back(value);
    std::sort(snake_items.begin(), snake_items.end());

    EXPECT_EQ(value_items, (std::vector<std::string>{"one", "three", "two"}));
    EXPECT_EQ(array_items, value_items);
    EXPECT_EQ(snake_items, value_items);
    EXPECT_EQ(values.len(), 3u);
    EXPECT_EQ(set.len(), 4u);
}

TEST(HashSetTest, SetRelations)
{
    HashSet<ca::i32> empty;
    HashSet<ca::i32> small    = {1, 2};
    HashSet<ca::i32> large    = {1, 2, 3, 4};
    HashSet<ca::i32> overlap  = {4, 5};
    HashSet<ca::i32> separate = {8, 9};

    EXPECT_TRUE(empty.is_subset_of(small));
    EXPECT_TRUE(empty.is_disjoint(small));

    EXPECT_TRUE(small.is_subset_of(large));
    EXPECT_TRUE(small.is_subset_of(large));
    EXPECT_FALSE(large.is_subset_of(small));

    EXPECT_TRUE(large.is_superset_of(small));
    EXPECT_TRUE(large.is_superset_of(small));
    EXPECT_FALSE(small.is_superset_of(large));

    EXPECT_FALSE(large.is_disjoint(overlap));
    EXPECT_FALSE(overlap.is_disjoint(large));
    EXPECT_TRUE(small.is_disjoint(separate));
    EXPECT_TRUE(separate.is_disjoint(small));
}

TEST(HashSetTest, SetOperationSnapshots)
{
    HashSet<ca::i32> left  = {1, 2, 3};
    HashSet<ca::i32> right = {3, 4, 5};

    auto union_set          = left.union_with(right);
    auto intersection_set   = left.intersection_with(right);
    auto difference_set     = left.difference_with(right);
    auto reverse_difference = right.difference_with(left);

    auto                 union_values = union_set.values();
    std::vector<ca::i32> union_items;
    for (const auto& value : union_values)
        union_items.push_back(value);
    std::sort(union_items.begin(), union_items.end());

    auto                 intersection_values = intersection_set.values();
    std::vector<ca::i32> intersection_items;
    for (const auto& value : intersection_values)
        intersection_items.push_back(value);
    std::sort(intersection_items.begin(), intersection_items.end());

    auto                 difference_values = difference_set.values();
    std::vector<ca::i32> difference_items;
    for (const auto& value : difference_values)
        difference_items.push_back(value);
    std::sort(difference_items.begin(), difference_items.end());

    auto                 reverse_difference_values = reverse_difference.values();
    std::vector<ca::i32> reverse_difference_items;
    for (const auto& value : reverse_difference_values)
        reverse_difference_items.push_back(value);
    std::sort(reverse_difference_items.begin(), reverse_difference_items.end());

    EXPECT_EQ(union_items, (std::vector<ca::i32>{1, 2, 3, 4, 5}));
    EXPECT_EQ(intersection_items, (std::vector<ca::i32>{3}));
    EXPECT_EQ(difference_items, (std::vector<ca::i32>{1, 2}));
    EXPECT_EQ(reverse_difference_items, (std::vector<ca::i32>{4, 5}));
    EXPECT_EQ(left.len(), 3u);
    EXPECT_EQ(right.len(), 3u);
}

TEST(HashSetTest, RetainAndRemoveIf)
{
    HashSet<ca::i32> set = {1, 2, 3, 4, 5, 6};

    EXPECT_EQ(set.remove_if([](ca::i32 value) { return value % 2 == 0; }), 3u);
    EXPECT_EQ(set.len(), 3u);
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(5));
    EXPECT_FALSE(set.contains(2));

    set.retain([](ca::i32 value) { return value >= 3; });
    EXPECT_EQ(set.len(), 2u);
    EXPECT_FALSE(set.contains(1));
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(5));

    EXPECT_EQ(set.remove_if([](ca::i32 value) { return value > 10; }), 0u);
    EXPECT_EQ(set.len(), 2u);
}

TEST(HashSetTest, SupportsMoveOnlyValues)
{
    HashSet<std::unique_ptr<ca::i32>> set;

    EXPECT_TRUE(set.add(std::make_unique<ca::i32>(1)));
    EXPECT_TRUE(set.insert(std::make_unique<ca::i32>(2)));

    ca::i32 sum = 0;
    for (const auto& value : set) {
        ASSERT_NE(value, nullptr);
        sum += *value;
    }

    EXPECT_EQ(set.len(), 2u);
    EXPECT_EQ(sum, 3);
}

TEST(HashSetTest, RemoveIfSupportsMoveOnlyValues)
{
    HashSet<std::unique_ptr<ca::i32>> set;
    set.add(std::make_unique<ca::i32>(1));
    set.add(std::make_unique<ca::i32>(2));
    set.add(std::make_unique<ca::i32>(3));

    EXPECT_EQ(set.remove_if([](const std::unique_ptr<ca::i32>& value) {
        return value != nullptr && *value == 2;
    }),
              1u);

    ca::i32 sum = 0;
    for (const auto& value : set) {
        ASSERT_NE(value, nullptr);
        sum += *value;
    }

    EXPECT_EQ(set.len(), 2u);
    EXPECT_EQ(sum, 4);
}

}   // namespace ca::collection
