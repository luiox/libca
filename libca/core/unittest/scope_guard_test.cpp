#include <gtest/gtest.h>

#include "libca/core/scope_guard.hpp"

#include <string>
#include <utility>

namespace ca::core::test {

TEST(ScopeGuardTest, RunsOnScopeExit) {
    bool called = false;
    {
        auto guard = make_scope_guard([&]() { called = true; });
        EXPECT_TRUE(guard.is_active());
    }
    EXPECT_TRUE(called);
}

TEST(ScopeGuardTest, DismissSkipsCallback) {
    bool called = false;
    {
        auto guard = make_scope_guard([&]() { called = true; });
        guard.dismiss();
        EXPECT_FALSE(guard.is_active());
    }
    EXPECT_FALSE(called);
}

TEST(ScopeGuardTest, MoveTransfersCallback) {
    int calls = 0;
    {
        auto first = make_scope_guard([&]() { ++calls; });
        auto second = std::move(first);
        EXPECT_FALSE(first.is_active());
        EXPECT_TRUE(second.is_active());
    }
    EXPECT_EQ(calls, 1);
}

TEST(ScopeGuardTest, DeferRunsAtScopeExit) {
    int value = 1;
    {
        DEFER(value += 2);
        EXPECT_EQ(value, 1);
    }
    EXPECT_EQ(value, 3);
}

TEST(ScopeGuardTest, DeferRunsInReverseDeclarationOrder) {
    std::string order;
    {
        DEFER(order += "b");
        DEFER(order += "a");
    }
    EXPECT_EQ(order, "ab");
}

} // namespace ca::core::test
