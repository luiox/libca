#include <gtest/gtest.h>
#include <libca/str/wstring.hpp>

#include <cwchar>

namespace ca::str {

namespace {
// 从字面量构造视图的测试辅助（WStringRef 无字面量构造，避免逐处手写长度）
WStringRef wref(const wchar_t* s) {
    return WStringRef(s, std::wcslen(s));
}
}  // namespace

TEST(WStringRefTest, Default) {
    WStringRef ref;
    EXPECT_TRUE(ref.is_empty());
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
    EXPECT_TRUE(s.is_empty());
}

TEST(WStringTest, FromWStr) {
    WString s(L"Hello");
    EXPECT_EQ(s.length(), 5);
}

TEST(WStringTest, Clone) {
    WString s1(L"Test");
    WString s2 = s1.clone();
    EXPECT_EQ(s2.length(), 4);
    EXPECT_NE(s1.data(), s2.data());
}

TEST(WStringTest, Move) {
    WString s1(L"Test");
    WString s2(std::move(s1));
    EXPECT_EQ(s2.length(), 4);
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

// ============================================================================
// WStringRef 操作族
// ============================================================================

TEST(WStringRefTest, StartsWithAndEndsWith) {
    WString s(L"Hello World");
    WStringRef ref = s.ref();
    EXPECT_TRUE(ref.starts_with(wref(L"Hello")));
    EXPECT_TRUE(ref.starts_with(wref(L"Hello World")));
    EXPECT_FALSE(ref.starts_with(wref(L"world")));
    EXPECT_FALSE(ref.starts_with(wref(L"Hello World!")));  // 比自身长
    EXPECT_TRUE(ref.starts_with(wref(L"")));               // 空前缀恒真
    EXPECT_TRUE(ref.ends_with(wref(L"World")));
    EXPECT_FALSE(ref.ends_with(wref(L"Hello")));
    EXPECT_TRUE(ref.ends_with(wref(L"")));
}

TEST(WStringRefTest, TrimFamily) {
    wchar_t buf[] = L" \t\r\n Hello World \t ";
    WStringRef ref(buf, 19);

    WStringRef both = ref.trim();
    EXPECT_TRUE(both == wref(L"Hello World"));
    EXPECT_TRUE(both.data() >= ref.data());
    EXPECT_TRUE(both.data() + both.length() <= ref.data() + ref.length());

    EXPECT_TRUE(ref.trim_start() == wref(L"Hello World \t "));
    EXPECT_TRUE(ref.trim_end() == wref(L" \t\r\n Hello World"));

    // 无空白时返回自身区间
    WStringRef clean = wref(L"abc");
    EXPECT_EQ(clean.trim().data(), clean.data());
    EXPECT_EQ(clean.trim().length(), 3);

    // 全空白修剪为空
    EXPECT_TRUE(wref(L" \t ").trim().is_empty());
}

TEST(WStringRefTest, Split) {
    WString s(L"a,b,,c,");
    std::vector<WStringRef> parts = s.ref().split(wref(L","));
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_TRUE(parts[0] == wref(L"a"));
    EXPECT_TRUE(parts[1] == wref(L"b"));
    EXPECT_TRUE(parts[2].is_empty());  // 连续分隔符产生空片段
    EXPECT_TRUE(parts[3] == wref(L"c"));
    EXPECT_TRUE(parts[4].is_empty());  // 结尾分隔符产生空片段

    EXPECT_TRUE(s.ref().split(wref(L"")).size() == 1u);   // 空分隔符返回自身
    EXPECT_TRUE(WString().ref().split(wref(L",")).empty());  // 空串返回空列表
    EXPECT_TRUE(s.ref().split(wref(L";")).size() == 1u);  // 无命中返回整串

    // 自由函数等价
    std::vector<WStringRef> via_free = split(s.ref(), wref(L","));
    ASSERT_EQ(via_free.size(), 5u);
    EXPECT_TRUE(via_free[0] == wref(L"a"));
}

TEST(WStringRefTest, ToLowerAndToUpper) {
    WString s(L"Hello World 42");
    EXPECT_TRUE(s.ref().to_lower() == wref(L"hello world 42"));
    EXPECT_TRUE(s.ref().to_upper() == wref(L"HELLO WORLD 42"));
    EXPECT_TRUE(WString().ref().to_lower().is_empty());

    // 原串不被修改
    EXPECT_TRUE(s == wref(L"Hello World 42"));
}

TEST(WStringRefTest, ReplaceAll) {
    WString s(L"banana");
    EXPECT_TRUE(s.ref().replace_all(wref(L"an"), wref(L"x")) == wref(L"bxxa"));
    // 从左到右、不重叠
    EXPECT_TRUE(wref(L"aaaa").replace_all(wref(L"aa"), wref(L"b")) == wref(L"bb"));
    // from 为空返回原内容拷贝
    WString copy = s.ref().replace_all(wref(L""), wref(L"zz"));
    EXPECT_TRUE(copy == wref(L"banana"));
    EXPECT_NE(copy.data(), s.data());
    // to 为空即删除
    EXPECT_TRUE(s.ref().replace_all(wref(L"an"), wref(L"")) == wref(L"ba"));
    // 无命中内容不变
    EXPECT_TRUE(s.ref().replace_all(wref(L"zz"), wref(L"y")) == wref(L"banana"));
}

// ============================================================================
// WString 操作族
// ============================================================================

TEST(WStringTest, StartsWithAndEndsWith) {
    WString s(L"prefix-body-suffix");
    EXPECT_TRUE(s.starts_with(wref(L"prefix-")));
    EXPECT_FALSE(s.starts_with(wref(L"Prefix-")));
    EXPECT_TRUE(s.ends_with(wref(L"-suffix")));
    EXPECT_FALSE(s.ends_with(wref(L"-Suffix")));
}

TEST(WStringTest, TrimAndSplitThroughOwningType) {
    WString s(L"  a b  ");
    EXPECT_TRUE(s.trim() == wref(L"a b"));
    EXPECT_TRUE(s.trim_start() == wref(L"a b  "));
    EXPECT_TRUE(s.trim_end() == wref(L"  a b"));

    std::vector<WStringRef> parts = s.split(wref(L" "));
    ASSERT_EQ(parts.size(), 6u);
    // 片段视图指向自身数据
    EXPECT_TRUE(parts[2] == wref(L"a"));
    EXPECT_TRUE(parts[3] == wref(L"b"));
    for (const WStringRef& part : parts) {
        EXPECT_TRUE(part.data() >= s.data());
        EXPECT_TRUE(part.data() + part.length() <= s.data() + s.length());
    }
}

TEST(WStringTest, ToLowerAndToUpperAndReplaceAll) {
    WString s(L"Mix Ed");
    EXPECT_TRUE(s.to_lower() == wref(L"mix ed"));
    EXPECT_TRUE(s.to_upper() == wref(L"MIX ED"));
    EXPECT_TRUE(s.replace_all(wref(L" "), wref(L"_")) == wref(L"Mix_Ed"));
    EXPECT_TRUE(s == wref(L"Mix Ed"));  // 原串不变
}

TEST(WStringBuilderTest, Clear) {
    WStringBuilder b;
    b.append(L"Hello");
    b.clear();
    EXPECT_TRUE(b.is_empty());
}

}  // namespace ca::str
