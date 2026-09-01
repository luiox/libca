#include <gmock/gmock.h>

#include "libca/core/any.hpp"

#include <memory>
#include <string>
#include <utility>

namespace ca::core {
namespace test {

using namespace testing;

namespace {
struct Point
{
    i32 x;
    i32 y;
};
}   // namespace

TEST(AnyTest, DefaultConstructed)
{
    Any a;
    EXPECT_FALSE(a.has_value());
}

TEST(AnyTest, HoldInt)
{
    Any a = 42;
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(a.is<int>());
    EXPECT_FALSE(a.is<double>());
    EXPECT_EQ(a.cast<int>(), 42);
}

TEST(AnyTest, HoldString)
{
    Any a = std::string("hello");
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(a.is<std::string>());
    EXPECT_EQ(a.cast<std::string>(), "hello");
}

TEST(AnyTest, CastWrite)
{
    Any a         = 10;
    a.cast<int>() = 99;
    EXPECT_EQ(a.cast<int>(), 99);
}

TEST(AnyTest, AsOk)
{
    Any   a = 42;
    auto* p = a.as<int>();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(AnyTest, AsMismatch)
{
    Any a = 42;
    EXPECT_EQ(a.as<double>(), nullptr);
}

TEST(AnyTest, AsEmpty)
{
    Any a;
    EXPECT_EQ(a.as<int>(), nullptr);
}

TEST(AnyTest, AsConst)
{
    const Any  a = 42;
    const int* p = a.as<int>();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(AnyTest, AsConstMismatch)
{
    const Any a = 42;
    EXPECT_EQ(a.as<double>(), nullptr);
}

TEST(AnyTest, CopyConstruct)
{
    Any a = 42;
    Any b(a);
    EXPECT_TRUE(b.is<int>());
    EXPECT_EQ(b.cast<int>(), 42);
    EXPECT_TRUE(a.is<int>());
    EXPECT_EQ(a.cast<int>(), 42);
}

TEST(AnyTest, MoveConstruct)
{
    Any a = 42;
    Any b(std::move(a));
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.is<int>());
    EXPECT_EQ(b.cast<int>(), 42);
}

TEST(AnyTest, CopyAssign)
{
    Any a = 42;
    Any b = std::string("world");
    b     = a;
    EXPECT_TRUE(b.is<int>());
    EXPECT_EQ(b.cast<int>(), 42);
    EXPECT_TRUE(a.is<int>());
}

TEST(AnyTest, MoveAssign)
{
    Any a = 42;
    Any b = std::string("world");
    b     = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.is<int>());
    EXPECT_EQ(b.cast<int>(), 42);
}

TEST(AnyTest, Reset)
{
    Any a = 3.14;
    EXPECT_TRUE(a.has_value());
    a.reset();
    EXPECT_FALSE(a.has_value());
}

TEST(AnyTest, CustomType)
{
    Any a = Point{1, 2};
    EXPECT_TRUE(a.is<Point>());
    EXPECT_EQ(a.cast<Point>().x, 1);
    EXPECT_EQ(a.cast<Point>().y, 2);
}

TEST(AnyTest, MoveOnly)
{
    Any a = std::make_unique<int>(42);
    EXPECT_TRUE(a.is<std::unique_ptr<int>>());
    EXPECT_EQ(*a.cast<std::unique_ptr<int>>(), 42);

    Any b(std::move(a));
    EXPECT_TRUE(b.is<std::unique_ptr<int>>());
    EXPECT_EQ(*b.cast<std::unique_ptr<int>>(), 42);
    EXPECT_FALSE(a.has_value());
}

TEST(AnyTest, MoveOnlyCopyYieldsEmpty)
{
    Any a = std::make_unique<int>(7);
    Any b(a);
    EXPECT_FALSE(b.has_value());
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(*a.cast<std::unique_ptr<int>>(), 7);
}

TEST(AnyTest, TypeTagStable)
{
    auto id_int    = type_id<int>();
    auto id_int2   = type_id<int>();
    auto id_double = type_id<double>();
    EXPECT_EQ(id_int, id_int2);
    EXPECT_NE(id_int, id_double);
}

TEST(AnyTest, SelfAssignment)
{
    Any a = 42;
    a     = a;
    EXPECT_TRUE(a.is<int>());
    EXPECT_EQ(a.cast<int>(), 42);
}

}   // namespace test
}   // namespace ca::core
