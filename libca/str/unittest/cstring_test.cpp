#include <gtest/gtest.h>
#include <libca/str/cstring.hpp>

namespace ca::str {

// ============================================================================
// CStringRef
// ============================================================================

TEST(CStringRefTest, DefaultConstructor) {
    CStringRef ref;
    EXPECT_TRUE(ref.isEmpty());
    EXPECT_EQ(ref.length(), 0);
}

TEST(CStringRefTest, FromData) {
    char data[] = "ABC";
    CStringRef ref(data, 3);
    EXPECT_EQ(ref.length(), 3);
    EXPECT_EQ(ref.at(0), 'A');
}

TEST(CStringRefTest, Slice) {
    char data[] = "Hello World";
    CStringRef ref(data, 11);
    auto s = ref.slice(0, 5);
    EXPECT_EQ(s.length(), 5);
    EXPECT_EQ(s.at(0), 'H');
}

TEST(CStringRefTest, Substr) {
    char data[] = "Hello World";
    CStringRef ref(data, 11);
    auto s = ref.substr(6, 5);
    EXPECT_EQ(s.length(), 5);
    EXPECT_STREQ(s.cStr(), "World");
}

TEST(CStringRefTest, Compare) {
    CStringRef r1("ABC", 3);
    CStringRef r2("ABD", 3);
    EXPECT_LT(r1.compare(r2), 0);
    EXPECT_EQ(r1.compare(r1), 0);
}

// ============================================================================
// CString
// ============================================================================

TEST(CStringTest, DefaultConstructor) {
    CString s;
    EXPECT_TRUE(s.isEmpty());
    EXPECT_EQ(s.cStr()[0], '\0');
}

TEST(CStringTest, FromCStr) {
    CString s("Hello World");
    EXPECT_EQ(s.length(), 11);
    EXPECT_STREQ(s.cStr(), "Hello World");
}

TEST(CStringTest, Clone) {
    CString s1("Test");
    CString s2 = s1.clone();
    EXPECT_EQ(s2.length(), 4);
    EXPECT_NE(s1.data(), s2.data());
}

TEST(CStringTest, Move) {
    CString s1("Test");
    CString s2(std::move(s1));
    EXPECT_EQ(s2.length(), 4);
    EXPECT_TRUE(s1.isEmpty());
}

TEST(CStringTest, Equality) {
    CString a("Hi"), b("Hi"), c("Bye");
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a == a.ref());
}

// ============================================================================
// CStringBuilder
// ============================================================================

TEST(CStringBuilderTest, Append) {
    CStringBuilder b;
    b.append("Hello").append(" World");
    EXPECT_EQ(b.length(), 11);
    EXPECT_STREQ(b.build().cStr(), "Hello World");
}

TEST(CStringBuilderTest, AppendChar) {
    CStringBuilder b;
    b.append('A').append('B').append('C');
    EXPECT_EQ(b.length(), 3);
    EXPECT_STREQ(b.build().cStr(), "ABC");
}

TEST(CStringBuilderTest, Clear) {
    CStringBuilder b;
    b.append("Hello");
    b.clear();
    EXPECT_TRUE(b.isEmpty());
}

TEST(CStringBuilderTest, BuildEmpty) {
    CStringBuilder b;
    EXPECT_TRUE(b.build().isEmpty());
}

}  // namespace ca::str
