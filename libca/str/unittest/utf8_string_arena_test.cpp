#include <gtest/gtest.h>
#include <libca/str/utf8_string_arena.hpp>

namespace ca::str {

// ============================================================================
// Utf8StringArena
// ============================================================================

TEST(Utf8StringArenaTest, EmptyIntern)
{
    Utf8StringArena arena;
    EXPECT_EQ(arena.size(), 0);

    auto r = arena.intern(nullptr, 0);
    EXPECT_TRUE(r.is_empty());

    r = arena.intern(static_cast<const char*>(nullptr));
    EXPECT_TRUE(r.is_empty());
}

TEST(Utf8StringArenaTest, InternCStr)
{
    Utf8StringArena arena;
    auto            r = arena.intern("Hello");
    EXPECT_EQ(r.length(), 5);
    EXPECT_EQ(r.byte_length(), 5);
    EXPECT_EQ(r.byte_at(0), 'H');
}

TEST(Utf8StringArenaTest, InternDedup)
{
    Utf8StringArena arena;
    auto            r1 = arena.intern("Hello");
    auto            r2 = arena.intern("Hello");
    EXPECT_EQ(r1.data(), r2.data());   // 指向同一块内存
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternMultiple)
{
    Utf8StringArena arena;
    auto            r1 = arena.intern("Hello");
    auto            r2 = arena.intern("World");
    EXPECT_NE(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 2);
}

TEST(Utf8StringArenaTest, InternUnicode)
{
    Utf8StringArena arena;
    auto            r = arena.intern("你好世界");
    EXPECT_EQ(r.length(), 4);
    EXPECT_EQ(r.byte_length(), 12);
}

TEST(Utf8StringArenaTest, InternUtf8StringRef)
{
    Utf8StringArena arena;
    u8              data[] = {0x41, 0x42, 0x43};
    Utf8StringRef   ref(data, 3, 3);
    auto            r = arena.intern(ref);
    EXPECT_EQ(r.length(), 3);
    EXPECT_EQ(r.byte_at(0), 0x41);
}

TEST(Utf8StringArenaTest, Move)
{
    Utf8StringArena a1;
    auto            r1 = a1.intern("Hello");

    Utf8StringArena a2(std::move(a1));
    EXPECT_EQ(a2.size(), 1);
    // r1 仍有效（指向 chunk 数据，chunk 被转移了）
    EXPECT_EQ(r1.length(), 5);
}

TEST(Utf8StringArenaTest, Clear)
{
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

TEST(Utf8StringArenaTest, TotalBytes)
{
    Utf8StringArena arena;
    EXPECT_GT(arena.total_bytes(), 0);   // 至少有一个 chunk

    arena.intern("Hello");
    EXPECT_GT(arena.total_bytes(), 0);
}

TEST(Utf8StringArenaTest, LargeBatch)
{
    Utf8StringArena arena;
    // 超过一个 chunk 大小
    std::string big;
    for (int i = 0; i < 5000; ++i)
        big += "Hello World ";
    auto r = arena.intern(big.c_str());
    EXPECT_EQ(r.length(), 5000 * 12);
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternSingleStringLargerThanChunk)
{
    Utf8StringArena arena;
    std::string     big;
    for (int i = 0; i < 70000; ++i)
        big += "A";

    auto r = arena.intern(big.c_str());
    EXPECT_EQ(r.length(), big.size());
    EXPECT_EQ(r.byte_length(), big.size());
    EXPECT_EQ(r.byte_at(0), 'A');
    EXPECT_EQ(r.byte_at(r.byte_length() - 1), 'A');
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternInvalidUtf8)
{
    Utf8StringArena arena;
    u8              bad[] = {0xFF, 0xFE};
    auto            r     = arena.intern(bad, 2);
    EXPECT_TRUE(r.is_empty());   // 非法 UTF-8 返回空
}

// ============================================================================
// intern_raw：不校验 UTF-8，按原始字节入池
// ============================================================================

TEST(Utf8StringArenaTest, InternRawAcceptsInvalidUtf8)
{
    // intern 对非法 UTF-8 返回空，intern_raw 应原样入池。
    Utf8StringArena arena;
    u8              carrier[] = {0xFF, 0xFE, 0x80, 0x00, 0xC0};   // 非 UTF-8
    auto            r         = arena.intern_raw(carrier, 5);
    EXPECT_FALSE(r.is_empty());
    EXPECT_EQ(r.byte_length(), 5u);
    EXPECT_EQ(r.length(), 5u);   // 保守值 = 字节长度
    // 字节逐位保持
    for (usize i = 0; i < 5; ++i) {
        EXPECT_EQ(r.byte_at(i), carrier[i]);
    }
}

TEST(Utf8StringArenaTest, InternRawDedup)
{
    Utf8StringArena arena;
    u8              a[] = {0xC2, 0x80, 0xFF};
    u8              b[] = {0xC2, 0x80, 0xFF};
    auto            r1  = arena.intern_raw(a, 3);
    auto            r2  = arena.intern_raw(b, 3);
    EXPECT_EQ(r1.data(), r2.data());   // 同内容去重，指向同一池内副本
    EXPECT_EQ(arena.size(), 1u);
}

TEST(Utf8StringArenaTest, InternRawHandlesEmptyAndNull)
{
    Utf8StringArena arena;
    EXPECT_TRUE(arena.intern_raw(nullptr, 0).is_empty());
    u8 x[] = {0x01};
    EXPECT_TRUE(arena.intern_raw(x, 0).is_empty());
}

// ============================================================================
// intern(const Utf8String&)
// ============================================================================

TEST(Utf8StringArenaTest, InternUtf8String)
{
    Utf8StringArena arena;
    Utf8String      s("Hello");
    auto            r = arena.intern(s);

    EXPECT_EQ(r.length(), 5);
    EXPECT_EQ(r.byte_length(), 5);
    EXPECT_EQ(r.byte_at(0), 'H');

    // 原始 Utf8String 仍有效
    EXPECT_EQ(s.length(), 5);
}

TEST(Utf8StringArenaTest, InternUtf8StringDedupWithCStr)
{
    Utf8StringArena arena;
    auto            r1 = arena.intern("Hello");
    Utf8String      s("Hello");
    auto            r2 = arena.intern(s);

    // 指向同一块内存（去重）
    EXPECT_EQ(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternUtf8StringDedupWithRef)
{
    Utf8StringArena arena;
    Utf8String      s1("Hello");
    auto            r1 = arena.intern(s1);
    Utf8String      s2("Hello");
    auto            r2 = arena.intern(s2);

    EXPECT_EQ(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 1);
}

TEST(Utf8StringArenaTest, InternUtf8StringMultiple)
{
    Utf8StringArena arena;
    Utf8String      s1("Hello");
    Utf8String      s2("World");

    auto r1 = arena.intern(s1);
    auto r2 = arena.intern(s2);

    EXPECT_NE(r1.data(), r2.data());
    EXPECT_EQ(arena.size(), 2);
}

TEST(Utf8StringArenaTest, InternUtf8StringUnicode)
{
    Utf8StringArena arena;
    Utf8String      s("你好世界");
    auto            r = arena.intern(s);

    EXPECT_EQ(r.length(), 4);
    EXPECT_EQ(r.byte_length(), 12);
}

TEST(Utf8StringArenaTest, InternUtf8StringEmpty)
{
    Utf8StringArena arena;
    Utf8String      s;
    auto            r = arena.intern(s);

    EXPECT_TRUE(r.is_empty());
    EXPECT_EQ(arena.size(), 0);
}

TEST(Utf8StringArenaTest, InternUtf8StringOriginalUnchanged)
{
    Utf8StringArena arena;
    Utf8String      s("Hello World");
    auto            dataBefore = s.data();

    auto r = arena.intern(s);

    // intern 后原始对象数据不变
    EXPECT_EQ(s.data(), dataBefore);
    EXPECT_EQ(s.length(), 11);
    EXPECT_EQ(s.byte_length(), 11);
}


TEST(Utf8StringArenaTest, InternStringView)
{
    Utf8StringArena   arena;
    const std::string generated = std::string("mjt$px") + std::to_string(42);
    auto              r         = arena.intern(std::string_view(generated));
    EXPECT_EQ(r.length(), 8);
    EXPECT_EQ(r.byte_length(), 8);

    // 与 cstr 路径同内容去重
    auto r2 = arena.intern("mjt$px42");
    EXPECT_EQ(r.data(), r2.data());
    EXPECT_EQ(arena.size(), 1);

    // 空视图
    EXPECT_TRUE(arena.intern(std::string_view()).is_empty());
}

TEST(Utf8StringArenaTest, OwnsInternedRef)
{
    Utf8StringArena arena;
    auto            r = arena.intern("owner/name");
    EXPECT_TRUE(arena.owns(r));

    // 去重命中返回的仍是池内指针
    auto r2 = arena.intern(std::string("owner/") + "name");
    EXPECT_TRUE(arena.owns(r2));
}

TEST(Utf8StringArenaTest, OwnsForeignOrTemporaryFalse)
{
    Utf8StringArena a;
    Utf8StringArena b;
    auto            ra = a.intern("in-a");
    auto            rb = b.intern("in-b");
    EXPECT_FALSE(a.owns(rb));
    EXPECT_FALSE(b.owns(ra));

    // 指向临时 std::string 的视图不属任何池
    const std::string tmp  = "temporary";
    auto              view = Utf8StringRef::from_string_view(tmp);
    EXPECT_FALSE(a.owns(view));

    // 空视图
    EXPECT_FALSE(a.owns(Utf8StringRef()));
}

TEST(Utf8StringArenaTest, OwnsRequiresCompleteViewRange)
{
    Utf8StringArena arena;
    std::vector<u8> first_chunk(64 * 1024, static_cast<u8>('a'));
    auto            stored = arena.intern_raw(first_chunk.data(), first_chunk.size());

    // 起点虽然在 chunk 内，但视图尾部已经越过 chunk，不能算本池视图。
    Utf8StringRef crossing(stored.data(), stored.byte_length() + 1, stored.length() + 1);
    EXPECT_FALSE(arena.owns(crossing));
}

TEST(Utf8StringArenaTest, OwnsAfterClearIsMeaningless)
{
    // clear 后旧 ref 悬垂（契约：语义前提是 ref 诞生后 arena 未回收）。
    // 本用例只验证 clear 后 owns 对空视图返回 false，不触碰悬垂指针。
    Utf8StringArena arena;
    (void)arena.intern("x");
    arena.clear();
    EXPECT_FALSE(arena.owns(Utf8StringRef()));
}

}   // namespace ca::str
