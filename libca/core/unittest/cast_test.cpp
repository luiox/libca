#include <gtest/gtest.h>
#include "libca/core/cast.hpp"

// Test types with TypeOf specialization
struct Base {
    enum Type { BASE, DERIVED_A, DERIVED_B };
    virtual ~Base() = default;
    virtual int getType() const { return BASE; }
};

struct DerivedA : Base {
    int getType() const override { return DERIVED_A; }
    int aValue() const { return 42; }
};

struct DerivedB : Base {
    int getType() const override { return DERIVED_B; }
    int bValue() const { return 99; }
};

template<> struct ca::core::TypeOf<DerivedA> : std::integral_constant<int, Base::DERIVED_A> {};
template<> struct ca::core::TypeOf<DerivedB> : std::integral_constant<int, Base::DERIVED_B> {};

TEST(TypedCastTest, isaMatchesCorrectType) {
    DerivedA a;
    DerivedB b;
    Base* baseA = &a;
    Base* baseB = &b;

    EXPECT_TRUE(typed::isa<DerivedA>(baseA));
    EXPECT_FALSE(typed::isa<DerivedB>(baseA));
    EXPECT_TRUE(typed::isa<DerivedB>(baseB));
    EXPECT_FALSE(typed::isa<DerivedA>(baseB));
}

TEST(TypedCastTest, isaNullReturnsFalse) {
    Base* nullPtr = nullptr;
    EXPECT_FALSE(typed::isa<DerivedA>(nullPtr));
}

TEST(TypedCastTest, castPerformsStaticDowncast) {
    DerivedA a;
    Base* base = &a;
    auto* casted = typed::cast<DerivedA>(base);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ(casted->aValue(), 42);
}

TEST(TypedCastTest, dynCastReturnsNullOnMismatch) {
    DerivedA a;
    DerivedB b;
    Base* baseA = &a;

    auto* result = typed::dyn_cast<DerivedB>(baseA);
    EXPECT_EQ(result, nullptr);
}

TEST(TypedCastTest, dynCastReturnsValidOnMatch) {
    DerivedA a;
    Base* base = &a;
    auto* result = typed::dyn_cast<DerivedA>(base);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->aValue(), 42);
}

TEST(TypedCastTest, isaWithConst) {
    DerivedA a;
    const Base* base = &a;
    EXPECT_TRUE(typed::isa<const DerivedA>(base));
    EXPECT_TRUE(typed::isa<DerivedA>(base));
}

TEST(TypedCastTest, dynCastWithConst) {
    const DerivedA a;
    const Base* base = &a;
    auto* result = typed::dyn_cast<const DerivedA>(base);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->aValue(), 42);
}
