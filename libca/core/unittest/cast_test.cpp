#include <gmock/gmock.h>

#include "libca/core/cast.hpp"

namespace ca::core {
namespace test {

using namespace testing;

struct Base : Polymorphic
{
    CA_TYPE_TAG(Base)
    virtual ~Base() = default;
};

struct DerivedA : Base
{
    CA_TYPE_TAG(DerivedA)
    int aValue() const { return 42; }
};

struct DerivedB : Base
{
    CA_TYPE_TAG(DerivedB)
    int bValue() const { return 99; }
};

TEST(TypedCastTest, isaMatchesCorrectType)
{
    DerivedA a;
    DerivedB b;
    Base*    baseA = &a;
    Base*    baseB = &b;

    EXPECT_TRUE(isa<DerivedA>(baseA));
    EXPECT_FALSE(isa<DerivedB>(baseA));
    EXPECT_TRUE(isa<DerivedB>(baseB));
    EXPECT_FALSE(isa<DerivedA>(baseB));
}

TEST(TypedCastTest, isaNullReturnsFalse)
{
    Base* nullPtr = nullptr;
    EXPECT_FALSE(isa<DerivedA>(nullPtr));
}

TEST(TypedCastTest, castPerformsStaticDowncast)
{
    DerivedA a;
    Base*    base   = &a;
    auto*    casted = cast<DerivedA>(base);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ(casted->aValue(), 42);
}

TEST(TypedCastTest, castOnConstPreservesConst)
{
    const DerivedA a;
    const Base*    base   = &a;
    auto*          result = cast<const DerivedA>(base);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->aValue(), 42);
}

TEST(TypedCastTest, dynCastReturnsNullOnMismatch)
{
    DerivedA a;
    DerivedB b;
    Base*    baseA = &a;

    auto* result = dyn_cast<DerivedB>(baseA);
    EXPECT_EQ(result, nullptr);
}

TEST(TypedCastTest, dynCastReturnsValidOnMatch)
{
    DerivedA a;
    Base*    base   = &a;
    auto*    result = dyn_cast<DerivedA>(base);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->aValue(), 42);
}

TEST(TypedCastTest, isaWithConst)
{
    DerivedA    a;
    const Base* base = &a;
    EXPECT_TRUE(isa<DerivedA>(base));
    EXPECT_FALSE(isa<DerivedB>(base));
}

TEST(TypedCastTest, dynCastWithConst)
{
    const DerivedA a;
    const Base*    base   = &a;
    auto*          result = dyn_cast<const DerivedA>(base);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->aValue(), 42);
}

// ============================================================================
// 实际场景：Base* 集合中判断实际类型
// ============================================================================

struct Animal : Polymorphic
{
    CA_TYPE_TAG(Animal)
    virtual ~Animal() = default;
    virtual const char* speak() const { return "?"; }
};

struct Dog : Animal
{
    CA_TYPE_TAG(Dog)
    const char* speak() const override { return "woof"; }
    int         fetch() const { return 42; }
};

struct Cat : Animal
{
    CA_TYPE_TAG(Cat)
    const char* speak() const override { return "meow"; }
    int         purr() const { return 99; }
};

TEST(TypedCastTest, CollectionOfBasePtrs)
{
    Dog                  d;
    Cat                  c;
    std::vector<Animal*> animals = {&d, &c};

    int fetch_sum = 0;
    int purr_sum  = 0;
    for (auto* a : animals) {
        if (isa<Dog>(a)) {
            fetch_sum += cast<Dog>(a)->fetch();
        }
        else if (isa<Cat>(a)) {
            purr_sum += cast<Cat>(a)->purr();
        }
    }
    EXPECT_EQ(fetch_sum, 42);
    EXPECT_EQ(purr_sum, 99);
}

TEST(TypedCastTest, isaExactMatchNotHierarchy)
{
    Dog     d;
    Animal* a = &d;

    // isa 是精确类型匹配：Dog 的实际类型是 Dog，不是 Animal
    EXPECT_TRUE(isa<Dog>(a));
    EXPECT_FALSE(isa<Animal>(a));
}

TEST(TypedCastTest, dynCastOnTypicalDispatch)
{
    Dog     d;
    Cat     c;
    Animal* animals[] = {&d, &c};

    auto* dog = dyn_cast<Dog>(animals[0]);
    ASSERT_NE(dog, nullptr);
    EXPECT_EQ(dog->fetch(), 42);

    auto* cat = dyn_cast<Cat>(animals[0]);
    EXPECT_EQ(cat, nullptr);
}

TEST(TypedCastTest, MultiLevelHierarchy)
{
    struct Mammal : Animal
    {
        CA_TYPE_TAG(Mammal)
    };
    struct Human : Mammal
    {
        CA_TYPE_TAG(Human)
        int speak_count = 0;
    };

    Human   h;
    Mammal* m = &h;
    Animal* a = &h;

    // 精确匹配：isa<Mammal> 在 Human 对象上返回 false
    EXPECT_FALSE(isa<Mammal>(m));
    EXPECT_FALSE(isa<Mammal>(a));
    EXPECT_TRUE(isa<Human>(m));
    EXPECT_TRUE(isa<Human>(a));
}

}   // namespace test
}   // namespace ca::core
