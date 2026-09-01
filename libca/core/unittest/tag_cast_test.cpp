#include <gtest/gtest.h>

#include "libca/core/tag_cast.hpp"

// 自包含类型层级：镜像 libjbytecode 系的 getType()/TypeOf 契约
// （节点以 int tag 区分，不依赖 RTTI）。
namespace {

struct Base
{
    int kind;
    explicit Base(int k)
        : kind(k)
    {}
    virtual ~Base() = default;
    int getType() const { return kind; }
};

struct Foo : Base
{
    Foo()
        : Base(1)
    {}
    int fooData = 42;
};
struct Bar : Base
{
    Bar()
        : Base(2)
    {}
    int barData = 99;
};

}   // namespace

namespace ca::core::tag_cast {
template<>
struct TypeOf<Foo>
{
    static constexpr int value = 1;
};
template<>
struct TypeOf<Bar>
{
    static constexpr int value = 2;
};
}   // namespace ca::core::tag_cast

// ---- cast: 无检查向下转型 ----

TEST(TagCastTest, castDowncastsToConcrete)
{
    Foo   foo;
    Base* base   = &foo;
    Foo*  casted = ca::core::tag_cast::cast<Foo*>(base);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ(casted->fooData, 42);
}

TEST(TagCastTest, castStripsCvAndStar)
{
    // T 可以是裸类型、指针或 const 形态——cleanup_t 全部归一到同一结果指针类型。
    Foo   foo;
    Base* base = &foo;
    EXPECT_EQ(ca::core::tag_cast::cast<Foo>(base), ca::core::tag_cast::cast<Foo*>(base));
    EXPECT_EQ(ca::core::tag_cast::cast<const Foo>(base), ca::core::tag_cast::cast<Foo*>(base));
}

TEST(TagCastTest, castConstOverloadPreservesConst)
{
    Foo         foo;
    const Base* base   = &foo;
    const Foo*  casted = ca::core::tag_cast::cast<Foo>(base);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ(casted->fooData, 42);
}

// ---- isa: 按 tag 判型 ----

TEST(TagIsaTest, isaTrueForMatchingType)
{
    Foo   foo;
    Base* base = &foo;
    EXPECT_TRUE(ca::core::tag_cast::isa<Foo>(base));
}

TEST(TagIsaTest, isaFalseForDifferentType)
{
    Bar   bar;
    Base* base = &bar;
    EXPECT_FALSE(ca::core::tag_cast::isa<Foo>(base));
    EXPECT_TRUE(ca::core::tag_cast::isa<Bar>(base));
}

TEST(TagIsaTest, isaFalseForNullptr)
{
    Base* base = nullptr;
    EXPECT_FALSE(ca::core::tag_cast::isa<Foo>(base));
}

// ---- dyn_cast: 受检向下转型 ----
// 注意：dyn_cast/isa 只有 const U* 重载，因此 dyn_cast 恒产出 const 目标指针
// （cast 另有非 const 重载，不受此限）。

TEST(TagDynCastTest, dynCastReturnsPointerOnMatch)
{
    Foo        foo;
    Base*      base   = &foo;
    const Foo* casted = ca::core::tag_cast::dyn_cast<Foo>(base);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ(casted->fooData, 42);
}

TEST(TagDynCastTest, dynCastReturnsNullOnMismatch)
{
    Bar   bar;
    Base* base = &bar;
    EXPECT_EQ(ca::core::tag_cast::dyn_cast<Foo>(base), nullptr);
}

TEST(TagDynCastTest, dynCastReturnsNullForNullptr)
{
    Base* base = nullptr;
    EXPECT_EQ(ca::core::tag_cast::dyn_cast<Foo>(base), nullptr);
}
