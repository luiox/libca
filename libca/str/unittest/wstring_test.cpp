#include <gtest/gtest.h>
#include <libca/str/wstring.hpp>

namespace ca::str {

TEST(WStringRefTest, Default) {
    WStringRef ref;
    EXPECT_TRUE(ref.isEmpty());
}

TEST(WStringRefTest, FromData) {
    wchar_t d[] = L"ABC";
    WStringRef ref(d, 3);
    EXPECT_EQ(ref.length(), 3);
    EXPECT_EQ(ref.at(0), L'A');
}

TEST(WStringRefTest, Slice) {
    wchar_t d[] = L"Hello World";
    WStringRef ref(d, 11);
    EXPECT_EQ(ref.slice(0, 5).length(), 5);
}

// ============================================================================

TEST(WStringTest, Default) {
    WString s;
    EXPECT_TRUE(s.isEmpty());
}

TEST(WStringTest, FromWStr) {
    WString s(L"Hello");
    EXPECT_EQ(s.length(), 5);
}

TEST(WStringTest, CopyMove) {
    WString s1(L"Test");
    WString s2(s1);
    EXPECT_EQ(s2.length(), 4);
    EXPECT_NE(s1.data(), s2.data());
    WString s3(std::move(s1));
    EXPECT_EQ(s3.length(), 4);
}

TEST(WStringTest, Equality) {
    WString a(L"Hi"), b(L"Hi"), c(L"Bye");
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}

// ============================================================================

TEST(WStringBuilderTest, Append) {
    WStringBuilder b;
    b.append(L"Hello").append(L" World");
    EXPECT_EQ(b.length(), 11);
}

TEST(WStringBuilderTest, AppendChar) {
    WStringBuilder b;
    b.append(L'A').append(L'B');
    EXPECT_EQ(b.length(), 2);
}

TEST(WStringBuilderTest, Clear) {
    WStringBuilder b;
    b.append(L"Hello");
    b.clear();
    EXPECT_TRUE(b.isEmpty());
}

}  // namespace ca::str
