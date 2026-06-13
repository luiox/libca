#include <gtest/gtest.h>
#include <libca/str/utf8_string_arena.hpp>

namespace ca::str {

// ============================================================================
// Utf8StringArena
// ============================================================================

TEST(Utf8StringArenaTest, EmptyIntern) {
    Utf8StringArena arena;
    EXPECT_EQ(arena.size(), 0);

    auto r = arena.intern(nullptr, 0);
    EXPECT_TRUE(r.is_empty());

    r = arena.intern(static_cast<const char*>(nullptr));
    EXPECT_TRUE(r.is_empty());
}

TEST(Utf8StringArenaTest, InternCStr) {
    Utf8StringArena arena;
    auto r = arena.intern("Hello");
    EXPECT_EQ(r.length(), 5);
    EXPECT_EQ(r.byte_length(), 5);
    EXPECT_EQ(r.byte_at(0), 'H');
}

TEST(Utf8StringArenaTest, InternDedup) {
    Utf8StringArena arena;
    auto r1 = arena.intern("Hello");
    auto r2 = arena.intern("Hello");
    EXPECT_EQ(r1.data(), r2.data());  // 指向同一块内存
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternMultiple) {
    Utf8StringArena arena;
    auto r1 = arena.intern("Hello");
    auto r2 = arena.intern("World");
    EXPECT_NE(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 2);
}

TEST(Utf8StringArenaTest, InternUnicode) {
    Utf8StringArena arena;
    auto r = arena.intern("你好世界");
    EXPECT_EQ(r.length(), 4);
    EXPECT_EQ(r.byte_length(), 12);
}

TEST(Utf8StringArenaTest, InternUtf8StringRef) {
    Utf8StringArena arena;
    u8 data[] = {0x41, 0x42, 0x43};
    Utf8StringRef ref(data, 3, 3);
    auto r = arena.intern(ref);
    EXPECT_EQ(r.length(), 3);
    EXPECT_EQ(r.byte_at(0), 0x41);
}

TEST(Utf8StringArenaTest, Move) {
    Utf8StringArena a1;
    auto r1 = a1.intern("Hello");

    Utf8StringArena a2(std::move(a1));
    EXPECT_EQ(a2.size(), 1);
    // r1 仍有效（指向 chunk 数据，chunk 被转移了）
    EXPECT_EQ(r1.length(), 5);
}

TEST(Utf8StringArenaTest, Clear) {
    Utf8StringArena arena;
    arena.intern("Hello");
    arena.intern("World");
    EXPECT_EQ(arena.size(), 2);

    arena.clear();
    EXPECT_EQ(arena.size(), 0);

    // 清空后可继续使用
    auto r = arena.intern("Hello");
    EXPECT_EQ(r.length(), 5);
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, TotalBytes) {
    Utf8StringArena arena;
    EXPECT_GT(arena.total_bytes(), 0);  // 至少有一个 chunk

    arena.intern("Hello");
    EXPECT_GT(arena.total_bytes(), 0);
}

TEST(Utf8StringArenaTest, LargeBatch) {
    Utf8StringArena arena;
    // 超过一个 chunk 大小
    std::string big;
    for (int i = 0; i < 5000; ++i)
        big += "Hello World ";
    auto r = arena.intern(big.c_str());
    EXPECT_EQ(r.length(), 5000 * 12);
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternInvalidUtf8) {
    Utf8StringArena arena;
    u8 bad[] = {0xFF, 0xFE};
    auto r = arena.intern(bad, 2);
    EXPECT_TRUE(r.is_empty());  // 非法 UTF-8 返回空
}

// ============================================================================
// intern(const Utf8String&)
// ============================================================================

TEST(Utf8StringArenaTest, InternUtf8String) {
    Utf8StringArena arena;
    Utf8String s("Hello");
    auto r = arena.intern(s);

    EXPECT_EQ(r.length(), 5);
    EXPECT_EQ(r.byte_length(), 5);
    EXPECT_EQ(r.byte_at(0), 'H');

    // 原始 Utf8String 仍有效
    EXPECT_EQ(s.length(), 5);
}

TEST(Utf8StringArenaTest, InternUtf8StringDedupWithCStr) {
    Utf8StringArena arena;
    auto r1 = arena.intern("Hello");
    Utf8String s("Hello");
    auto r2 = arena.intern(s);

    // 指向同一块内存（去重）
    EXPECT_EQ(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternUtf8StringDedupWithRef) {
    Utf8StringArena arena;
    Utf8String s1("Hello");
    auto r1 = arena.intern(s1);
    Utf8String s2("Hello");
    auto r2 = arena.intern(s2);

    EXPECT_EQ(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternUtf8StringMultiple) {
    Utf8StringArena arena;
    Utf8String s1("Hello");
    Utf8String s2("World");

    auto r1 = arena.intern(s1);
    auto r2 = arena.intern(s2);

    EXPECT_NE(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 2);
}

TEST(Utf8StringArenaTest, InternUtf8StringUnicode) {
    Utf8StringArena arena;
    Utf8String s("你好世界");
    auto r = arena.intern(s);

    EXPECT_EQ(r.length(), 4);
    EXPECT_EQ(r.byte_length(), 12);
}

TEST(Utf8StringArenaTest, InternUtf8StringEmpty) {
    Utf8StringArena arena;
    Utf8String s;
    auto r = arena.intern(s);

    EXPECT_TRUE(r.is_empty());
    EXPECT_EQ(arena.size(), 0);
}

TEST(Utf8StringArenaTest, InternUtf8StringOriginalUnchanged) {
    Utf8StringArena arena;
    Utf8String s("Hello World");
    auto dataBefore = s.data();

    auto r = arena.intern(s);

    // intern 后原始对象数据不变
    EXPECT_EQ(s.data(), dataBefore);
    EXPECT_EQ(s.length(), 11);
    EXPECT_EQ(s.byte_length(), 11);
}

}  // namespace ca::str
