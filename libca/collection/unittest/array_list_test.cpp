#include "libca/collection/array_list.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ca::collection {

TEST(ArrayListTest, AddGetSetAndIterate)
{
    auto list = ArrayList<std::string>::with_capacity(4);

    EXPECT_TRUE(list.is_empty());
    EXPECT_GE(list.capacity(), 4u);

    list.add("alpha");
    list.add(std::string("beta"));
    list.push("gamma");
    list.set(1, "BETA");

    ASSERT_EQ(list.len(), 3u);
    EXPECT_EQ(list.get(0), "alpha");
    EXPECT_EQ(list[1], "BETA");
    EXPECT_EQ(list.get(2), "gamma");

    std::vector<std::string> values;
    for (const auto& value : list)
        values.push_back(value);

    EXPECT_EQ(values, (std::vector<std::string>{"alpha", "BETA", "gamma"}));
}

TEST(ArrayListTest, FromSliceAndBulkAppend)
{
    const ca::i32 raw[] = {1, 2, 3};
    auto list = ArrayList<ca::i32>::from_slice(raw, 3);

    ASSERT_EQ(list.len(), 3u);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(2), 3);

    const ca::i32 more[] = {4, 5};
    list.extend_from_slice(more, 2);
    list.extend_from_slice(nullptr, 0);
    EXPECT_EQ(list.len(), 5u);
    EXPECT_EQ(list.get(3), 4);
    EXPECT_EQ(list.get(4), 5);

    auto other = ArrayList<ca::i32>::from_slice(raw, 2);
    list.add_all(other);
    list.add_all(ArrayList<ca::i32>{9});

    EXPECT_EQ(list.len(), 8u);
    EXPECT_EQ(list.get(5), 1);
    EXPECT_EQ(list.get(6), 2);
    EXPECT_EQ(list.get(7), 9);
    EXPECT_THROW(list.extend_from_slice(nullptr, 1), std::invalid_argument);
}

TEST(ArrayListTest, SliceAndSubListReturnSnapshot)
{
    ArrayList<std::string> list = {"alpha", "beta", "gamma", "delta"};

    auto middle = list.slice(1, 3);
    ASSERT_EQ(middle.len(), 2u);
    EXPECT_EQ(middle.get(0), "beta");
    EXPECT_EQ(middle.get(1), "gamma");

    middle.set(0, "BETA");
    EXPECT_EQ(middle.get(0), "BETA");
    EXPECT_EQ(list.get(1), "beta");

    auto prefix = list.sub_list(0, 2);
    EXPECT_EQ(prefix.len(), 2u);
    EXPECT_EQ(prefix.get(0), "alpha");
    EXPECT_EQ(prefix.get(1), "beta");

    auto suffix = list.sub_list(2, 4);
    EXPECT_EQ(suffix.len(), 2u);
    EXPECT_EQ(suffix.get(0), "gamma");
    EXPECT_EQ(suffix.get(1), "delta");

    EXPECT_TRUE(list.slice(2, 2).is_empty());
    EXPECT_THROW(list.slice(3, 2), std::out_of_range);
    EXPECT_THROW(list.slice(0, 5), std::out_of_range);
}

TEST(ArrayListTest, InsertRemoveAndPop)
{
    ArrayList<ca::i32> list = {1, 3};

    list.insert(1, 2);
    list.insert(list.len(), 4);

    ASSERT_EQ(list.len(), 4u);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);
    EXPECT_EQ(list.get(2), 3);
    EXPECT_EQ(list.get(3), 4);

    EXPECT_EQ(list.remove_at(1), 2);
    EXPECT_EQ(list.len(), 3u);

    auto popped = list.pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(*popped, 4);
    EXPECT_EQ(list.len(), 2u);

    list.clear();
    EXPECT_TRUE(list.is_empty());
    EXPECT_FALSE(list.pop().has_value());
}

TEST(ArrayListTest, TruncateShortensWithoutGrowing)
{
    ArrayList<ca::i32> list = {1, 2, 3, 4};

    list.truncate(2);
    ASSERT_EQ(list.len(), 2u);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);

    list.truncate(2);
    ASSERT_EQ(list.len(), 2u);

    list.truncate(8);
    ASSERT_EQ(list.len(), 2u);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);

    list.truncate(0);
    EXPECT_TRUE(list.is_empty());
}

TEST(ArrayListTest, ResizeChangesLengthAndFillsNewValues)
{
    ArrayList<ca::i32> list = {1, 2};

    list.resize(4, 9);
    ASSERT_EQ(list.len(), 4u);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);
    EXPECT_EQ(list.get(2), 9);
    EXPECT_EQ(list.get(3), 9);

    list.resize(6);
    ASSERT_EQ(list.len(), 6u);
    EXPECT_EQ(list.get(4), 0);
    EXPECT_EQ(list.get(5), 0);

    list.resize(1);
    ASSERT_EQ(list.len(), 1u);
    EXPECT_EQ(list.get(0), 1);
}

TEST(ArrayListTest, SwapExchangesValuesAndChecksBounds)
{
    ArrayList<std::string> list = {"alpha", "beta", "gamma"};

    list.swap(0, 2);
    EXPECT_EQ(list.get(0), "gamma");
    EXPECT_EQ(list.get(1), "beta");
    EXPECT_EQ(list.get(2), "alpha");

    list.swap(1, 1);
    EXPECT_EQ(list.get(1), "beta");

    EXPECT_THROW(list.swap(0, 3), std::out_of_range);
    EXPECT_THROW(list.swap(3, 0), std::out_of_range);
}

TEST(ArrayListTest, SwapRemoveTakesValueWithoutPreservingOrder)
{
    ArrayList<std::string> list = {"alpha", "beta", "gamma", "delta"};

    auto removed = list.swap_remove(1);

    EXPECT_EQ(removed, "beta");
    ASSERT_EQ(list.len(), 3u);
    EXPECT_EQ(list.get(0), "alpha");
    EXPECT_EQ(list.get(1), "delta");
    EXPECT_EQ(list.get(2), "gamma");

    auto last = list.swap_remove(2);
    EXPECT_EQ(last, "gamma");
    ASSERT_EQ(list.len(), 2u);
    EXPECT_EQ(list.get(0), "alpha");
    EXPECT_EQ(list.get(1), "delta");

    EXPECT_THROW(list.swap_remove(2), std::out_of_range);
}

TEST(ArrayListTest, SearchAndRemoveValue)
{
    ArrayList<std::string> list = {"alpha", "beta", "gamma", "beta"};

    EXPECT_TRUE(list.contains("beta"));
    EXPECT_FALSE(list.contains("missing"));
    EXPECT_EQ(list.index_of("beta"), 1u);
    EXPECT_EQ(list.index_of("gamma"), 2u);
    EXPECT_EQ(list.last_index_of("beta"), 3u);
    EXPECT_EQ(list.last_index_of("missing"), ArrayList<std::string>::npos);

    EXPECT_TRUE(list.remove_value("beta"));
    ASSERT_EQ(list.len(), 3u);
    EXPECT_EQ(list.get(0), "alpha");
    EXPECT_EQ(list.get(1), "gamma");
    EXPECT_EQ(list.get(2), "beta");

    EXPECT_TRUE(list.remove_value("beta"));
    EXPECT_FALSE(list.contains("beta"));
    EXPECT_FALSE(list.remove_value("beta"));
}

TEST(ArrayListTest, RetainAndRemoveIf)
{
    ArrayList<ca::i32> list = {1, 2, 3, 4, 5, 6};

    EXPECT_EQ(list.remove_if([](ca::i32 value) { return value % 2 == 0; }), 3u);
    ASSERT_EQ(list.len(), 3u);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 3);
    EXPECT_EQ(list.get(2), 5);

    list.retain([](ca::i32 value) { return value >= 3; });
    ASSERT_EQ(list.len(), 2u);
    EXPECT_EQ(list.get(0), 3);
    EXPECT_EQ(list.get(1), 5);

    EXPECT_EQ(list.remove_if([](ca::i32 value) { return value > 10; }), 0u);
    EXPECT_EQ(list.len(), 2u);
}

TEST(ArrayListTest, TryGetReturnsPointerOrNull)
{
    ArrayList<std::string> list = {"alpha", "beta"};

    auto* value = list.try_get(1);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, "beta");

    *value = "BETA";
    EXPECT_EQ(list.get(1), "BETA");

    const auto& const_list = list;
    const auto* const_value = const_list.try_get(0);
    ASSERT_NE(const_value, nullptr);
    EXPECT_EQ(*const_value, "alpha");

    EXPECT_EQ(list.try_get(2), nullptr);
    EXPECT_EQ(const_list.try_get(2), nullptr);
}

TEST(ArrayListTest, FirstAndLastReturnPointersOrNull)
{
    ArrayList<std::string> list;

    EXPECT_EQ(list.first(), nullptr);
    EXPECT_EQ(list.last(), nullptr);

    list.add("alpha");
    list.add("beta");

    ASSERT_NE(list.first(), nullptr);
    ASSERT_NE(list.last(), nullptr);
    EXPECT_EQ(*list.first(), "alpha");
    EXPECT_EQ(*list.last(), "beta");

    *list.first() = "ALPHA";
    *list.last() = "BETA";
    EXPECT_EQ(list.get(0), "ALPHA");
    EXPECT_EQ(list.get(1), "BETA");

    const auto& const_list = list;
    ASSERT_NE(const_list.first(), nullptr);
    ASSERT_NE(const_list.last(), nullptr);
    EXPECT_EQ(*const_list.first(), "ALPHA");
    EXPECT_EQ(*const_list.last(), "BETA");
}

TEST(ArrayListTest, BoundsChecks)
{
    ArrayList<ca::i32> list = {1, 2};

    EXPECT_THROW(list.get(2), std::out_of_range);
    EXPECT_THROW(list.set(2, 3), std::out_of_range);
    EXPECT_THROW(list.insert(3, 3), std::out_of_range);
    EXPECT_THROW(list.remove_at(2), std::out_of_range);
}

TEST(ArrayListTest, SupportsMoveOnlyValues)
{
    ArrayList<std::unique_ptr<ca::i32>> list;
    list.add(std::make_unique<ca::i32>(1));
    list.emplace(new ca::i32(2));

    ASSERT_EQ(list.len(), 2u);
    ASSERT_NE(list.get(0), nullptr);
    ASSERT_NE(list.get(1), nullptr);
    EXPECT_EQ(*list.get(0), 1);
    EXPECT_EQ(*list.get(1), 2);

    auto removed = list.remove_at(0);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(*removed, 1);
    EXPECT_EQ(list.len(), 1u);

    list.truncate(0);
    EXPECT_TRUE(list.is_empty());

    list.resize(2);
    ASSERT_EQ(list.len(), 2u);
    EXPECT_EQ(list.get(0), nullptr);
    EXPECT_EQ(list.get(1), nullptr);

    list.set(0, std::make_unique<ca::i32>(10));
    list.set(1, std::make_unique<ca::i32>(20));
    list.swap(0, 1);
    ASSERT_NE(list.get(0), nullptr);
    ASSERT_NE(list.get(1), nullptr);
    EXPECT_EQ(*list.get(0), 20);
    EXPECT_EQ(*list.get(1), 10);

    auto taken = list.swap_remove(0);
    ASSERT_NE(taken, nullptr);
    EXPECT_EQ(*taken, 20);
    ASSERT_EQ(list.len(), 1u);
    ASSERT_NE(list.get(0), nullptr);
    EXPECT_EQ(*list.get(0), 10);
}

TEST(ArrayListTest, RemoveIfSupportsMoveOnlyValues)
{
    ArrayList<std::unique_ptr<ca::i32>> list;
    list.add(std::make_unique<ca::i32>(1));
    list.add(std::make_unique<ca::i32>(2));
    list.add(std::make_unique<ca::i32>(3));

    EXPECT_EQ(list.remove_if([](const std::unique_ptr<ca::i32>& value) {
        return value != nullptr && *value == 2;
    }), 1u);

    ASSERT_EQ(list.len(), 2u);
    ASSERT_NE(list.get(0), nullptr);
    ASSERT_NE(list.get(1), nullptr);
    EXPECT_EQ(*list.get(0), 1);
    EXPECT_EQ(*list.get(1), 3);
}

}  // namespace ca::collection
