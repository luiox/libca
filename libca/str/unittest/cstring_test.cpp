#include <gtest/gtest.h>
#include <libca/str/cstring.hpp>

#include <cstring>

namespace ca::str {

namespace {
// 从字面量构造视图的测试辅助（CStringRef 无字面量构造，避免逐处手写长度）
CStringRef cref(const char* s) {
    return CStringRef(s, std::strlen(s));
}
}  // namespace

// ============================================================================
// CStringRef
// ============================================================================

TEST(CStringRefTest, DefaultConstructor) {
    CStringRef ref;
    EXPECT_TRUE(ref.is_empty());
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
    EXPECT_STREQ(s.c_str(), "World");
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
    EXPECT_TRUE(s.is_empty());
    EXPECT_EQ(s.c_str()[0], '\0');
}

TEST(CStringTest, FromCStr) {
    CString s("Hello World");
    EXPECT_EQ(s.length(), 11);
    EXPECT_STREQ(s.c_str(), "Hello World");
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
    EXPECT_TRUE(s1.is_empty());
}

TEST(CStringTest, Equality) {
    CString a("Hi"), b("Hi"), c("Bye");
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a == a.ref());
}

// ============================================================================
// CStringRef 操作族
// ============================================================================

TEST(CStringRefTest, StartsWithAndEndsWith) {
    CString s("Hello World");
    CStringRef ref = s.ref();
    EXPECT_TRUE(ref.starts_with(cref("Hello")));
    EXPECT_TRUE(ref.starts_with(cref("Hello World")));
    EXPECT_FALSE(ref.starts_with(cref("world")));
    EXPECT_FALSE(ref.starts_with(cref("Hello World!")));  // 比自身长
    EXPECT_TRUE(ref.starts_with(cref("")));               // 空前缀恒真
    EXPECT_TRUE(ref.ends_with(cref("World")));
    EXPECT_FALSE(ref.ends_with(cref("Hello")));
    EXPECT_TRUE(ref.ends_with(cref("")));
}

TEST(CStringRefTest, TrimFamily) {
    char buf[] = " \t\r\n Hello World \t ";
    CStringRef ref(buf, 19);

    CStringRef both = ref.trim();
    EXPECT_TRUE(both == cref("Hello World"));
    EXPECT_TRUE(both.data() >= ref.data());
    EXPECT_TRUE(both.data() + both.length() <= ref.data() + ref.length());

    EXPECT_TRUE(ref.trim_start() == cref("Hello World \t "));
    EXPECT_TRUE(ref.trim_end() == cref(" \t\r\n Hello World"));

    // 无空白时返回自身区间
    CStringRef clean = cref("abc");
    EXPECT_EQ(clean.trim().data(), clean.data());
    EXPECT_EQ(clean.trim().length(), 3);

    // 全空白修剪为空
    EXPECT_TRUE(cref(" \t ").trim().is_empty());
}

TEST(CStringRefTest, Split) {
    CString s("a,b,,c,");
    std::vector<CStringRef> parts = s.ref().split(cref(","));
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_TRUE(parts[0] == cref("a"));
    EXPECT_TRUE(parts[1] == cref("b"));
    EXPECT_TRUE(parts[2].is_empty());  // 连续分隔符产生空片段
    EXPECT_TRUE(parts[3] == cref("c"));
    EXPECT_TRUE(parts[4].is_empty());  // 结尾分隔符产生空片段

    EXPECT_EQ(s.ref().split(cref("")).size(), 1u);        // 空分隔符返回自身
    EXPECT_TRUE(CString().ref().split(cref(",")).empty());  // 空串返回空列表
    EXPECT_EQ(s.ref().split(cref(";")).size(), 1u);       // 无命中返回整串

    // 自由函数等价
    std::vector<CStringRef> via_free = split(s.ref(), cref(","));
    ASSERT_EQ(via_free.size(), 5u);
    EXPECT_TRUE(via_free[0] == cref("a"));
}

TEST(CStringRefTest, ToLowerAndToUpper) {
    CString s("Hello World 42");
    EXPECT_STREQ(s.ref().to_lower().c_str(), "hello world 42");
    EXPECT_STREQ(s.ref().to_upper().c_str(), "HELLO WORLD 42");
    EXPECT_TRUE(CString().ref().to_lower().is_empty());

    // 原串不被修改
    EXPECT_STREQ(s.c_str(), "Hello World 42");
}

TEST(CStringRefTest, ReplaceAll) {
    CString s("banana");
    EXPECT_STREQ(s.ref().replace_all(cref("an"), cref("x")).c_str(), "bxxa");
    // 从左到右、不重叠
    EXPECT_STREQ(cref("aaaa").replace_all(cref("aa"), cref("b")).c_str(), "bb");
    // from 为空返回原内容拷贝
    CString copy = s.ref().replace_all(cref(""), cref("zz"));
    EXPECT_STREQ(copy.c_str(), "banana");
    EXPECT_NE(copy.data(), s.data());
    // to 为空即删除
    EXPECT_STREQ(s.ref().replace_all(cref("an"), cref("")).c_str(), "ba");
    // 无命中内容不变
    EXPECT_STREQ(s.ref().replace_all(cref("zz"), cref("y")).c_str(), "banana");
}

// ============================================================================
// CString 操作族
// ============================================================================

TEST(CStringTest, StartsWithAndEndsWith) {
    CString s("prefix-body-suffix");
    EXPECT_TRUE(s.starts_with(cref("prefix-")));
    EXPECT_FALSE(s.starts_with(cref("Prefix-")));
    EXPECT_TRUE(s.ends_with(cref("-suffix")));
    EXPECT_FALSE(s.ends_with(cref("-Suffix")));
}

TEST(CStringTest, TrimAndSplitThroughOwningType) {
    CString s("  a b  ");
    EXPECT_TRUE(s.trim() == cref("a b"));
    EXPECT_TRUE(s.trim_start() == cref("a b  "));
    EXPECT_TRUE(s.trim_end() == cref("  a b"));

    std::vector<CStringRef> parts = s.split(cref(" "));
    ASSERT_EQ(parts.size(), 6u);
    // 片段视图指向自身数据
    EXPECT_TRUE(parts[2] == cref("a"));
    EXPECT_TRUE(parts[3] == cref("b"));
    for (const CStringRef& part : parts) {
        EXPECT_TRUE(part.data() >= s.data());
        EXPECT_TRUE(part.data() + part.length() <= s.data() + s.length());
    }
}

TEST(CStringTest, ToLowerAndToUpperAndReplaceAll) {
    CString s("Mix Ed");
    EXPECT_STREQ(s.to_lower().c_str(), "mix ed");
    EXPECT_STREQ(s.to_upper().c_str(), "MIX ED");
    EXPECT_STREQ(s.replace_all(cref(" "), cref("_")).c_str(), "Mix_Ed");
    EXPECT_STREQ(s.c_str(), "Mix Ed");  // 原串不变
}

// ============================================================================
// CStringBuilder
// ============================================================================

TEST(CStringBuilderTest, Append) {
    CStringBuilder b;
    b.append("Hello").append(" World");
    EXPECT_EQ(b.length(), 11);
    EXPECT_STREQ(b.build().c_str(), "Hello World");
}

TEST(CStringBuilderTest, AppendChar) {
    CStringBuilder b;
    b.append('A').append('B').append('C');
    EXPECT_EQ(b.length(), 3);
    EXPECT_STREQ(b.build().c_str(), "ABC");
}

TEST(CStringBuilderTest, Clear) {
    CStringBuilder b;
    b.append("Hello");
    b.clear();
    EXPECT_TRUE(b.is_empty());
}

TEST(CStringBuilderTest, BuildEmpty) {
    CStringBuilder b;
    EXPECT_TRUE(b.build().is_empty());
}

}  // namespace ca::str
