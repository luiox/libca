#include <gtest/gtest.h>
#include <libca/str/utf8_string_pool.hpp>

namespace ca::str {

// ============================================================================
// Utf8StringPooledPtr
// ============================================================================

TEST(Utf8StringPooledPtrTest, DefaultEmpty) {
    Utf8StringPooledPtr p;
    EXPECT_TRUE(p.is_empty());
    EXPECT_EQ(p.byte_length(), 0);
    EXPECT_EQ(p.length(), 0);
    EXPECT_FALSE(p);
}

TEST(Utf8StringPooledPtrTest, DataAccess) {
    Utf8StringPool pool;
    auto p = pool.intern("Hello");
    EXPECT_TRUE(p);
    EXPECT_FALSE(p.is_empty());
    EXPECT_EQ(p.length(), 5);
    EXPECT_EQ(p.byte_length(), 5);
    EXPECT_EQ(p.data()[0], 'H');
}

TEST(Utf8StringPooledPtrTest, CopyRefCount) {
    Utf8StringPool pool;
    {
        auto p1 = pool.intern("Hello");
        EXPECT_EQ(pool.active_entries(), 1);

        {
            auto p2 = p1;  // 拷贝
            EXPECT_EQ(pool.active_entries(), 1);
        }
        // p2 析构，refCount 恢复
        EXPECT_EQ(pool.active_entries(), 1);
    }
    // p1 析构，refCount=0，条目死亡
    EXPECT_EQ(pool.active_entries(), 0);
}

TEST(Utf8StringPooledPtrTest, MoveNoRefCountChange) {
    Utf8StringPool pool;
    auto p1 = pool.intern("Hello");
    EXPECT_EQ(pool.active_entries(), 1);

    auto p2 = std::move(p1);
    // p1 转移给 p2，refCount 不变
    EXPECT_FALSE(p1);       // p1 空了
    EXPECT_TRUE(p2);
    EXPECT_EQ(pool.active_entries(), 1);

    // p2 析构
}

TEST(Utf8StringPooledPtrTest, Assignment) {
    Utf8StringPool pool;
    auto p1 = pool.intern("Hello");
    auto p2 = pool.intern("World");

    EXPECT_EQ(pool.active_entries(), 2);

    p2 = p1;  // p2 指向 "Hello"，"World" 被释放
    EXPECT_EQ(pool.active_entries(), 1);
    EXPECT_EQ(p2.length(), 5);
}

TEST(Utf8StringPooledPtrTest, Ref) {
    Utf8StringPool pool;
    auto p = pool.intern("Hello");
    auto ref = p.ref();
    EXPECT_EQ(ref.length(), 5);
    EXPECT_EQ(ref.byte_length(), 5);
}

TEST(Utf8StringPooledPtrTest, Compare) {
    Utf8StringPool pool;
    auto p1 = pool.intern("Hello");
    auto p2 = pool.intern("Hello");
    EXPECT_TRUE(p1 == p2);  // 同一条目
    EXPECT_FALSE(p1 != p2);

    auto p3 = pool.intern("World");
    EXPECT_FALSE(p1 == p3);
    EXPECT_TRUE(p1 != p3);
}


// ============================================================================
// Utf8StringPool
// ============================================================================

TEST(Utf8StringPoolTest, BasicIntern) {
    Utf8StringPool pool;
    auto p = pool.intern("Hello World");
    EXPECT_FALSE(p.is_empty());
    EXPECT_EQ(p.length(), 11);
}

TEST(Utf8StringPoolTest, InternDedup) {
    Utf8StringPool pool;
    auto p1 = pool.intern("Hello");
    auto p2 = pool.intern("Hello");
    // 相同内容返回同一条目（指针相等）
    EXPECT_EQ(p1.data(), p2.data());
    EXPECT_EQ(pool.size(), 1);    // 只有 1 个条目
    EXPECT_EQ(pool.active_entries(), 1);
}

TEST(Utf8StringPoolTest, InternDifferent) {
    Utf8StringPool pool;
    auto p1 = pool.intern("Hello");
    auto p2 = pool.intern("World");
    EXPECT_NE(p1.data(), p2.data());
    EXPECT_EQ(pool.size(), 2);
    EXPECT_EQ(pool.active_entries(), 2);
}

TEST(Utf8StringPoolTest, InternEmpty) {
    Utf8StringPool pool;
    auto p = pool.intern(static_cast<const char*>(nullptr));
    EXPECT_FALSE(p);

    p = pool.intern(static_cast<const u8*>(nullptr), 0);
    EXPECT_FALSE(p);
}

TEST(Utf8StringPoolTest, InternUtf8StringRef) {
    Utf8StringPool pool;
    u8 data[] = {0xE4, 0xB8, 0xAD};  // '中'
    Utf8StringRef ref(data, 3, 1);
    auto p = pool.intern(ref);
    EXPECT_EQ(p.length(), 1);
    EXPECT_EQ(p.byte_length(), 3);
}

TEST(Utf8StringPoolTest, InternUnicode) {
    Utf8StringPool pool;
    auto p = pool.intern("你好世界");
    EXPECT_EQ(p.length(), 4);
    EXPECT_EQ(p.byte_length(), 12);
}

TEST(Utf8StringPoolTest, InternInvalidUtf8) {
    Utf8StringPool pool;
    u8 bad[] = {0xFF, 0xFE};
    auto p = pool.intern(bad, 2);
    EXPECT_FALSE(p);  // 非法 UTF-8
}

TEST(Utf8StringPoolTest, Clear) {
    Utf8StringPool pool;
    pool.intern("Hello");
    pool.intern("World");
    EXPECT_EQ(pool.size(), 2);

    pool.clear();
    EXPECT_EQ(pool.size(), 0);
    EXPECT_EQ(pool.active_entries(), 0);

    // 清空后可继续使用
    auto p = pool.intern("Hello");
    EXPECT_TRUE(p);
    EXPECT_EQ(pool.size(), 1);
}

TEST(Utf8StringPoolTest, TotalBytes) {
    Utf8StringPool pool;
    auto h = pool.intern("Hello");
    auto w = pool.intern("World");
    EXPECT_EQ(pool.total_bytes(), 10);
}

TEST(Utf8StringPoolTest, Move) {
    Utf8StringPool p1;
    auto ptr = p1.intern("Hello");

    Utf8StringPool p2(std::move(p1));
    EXPECT_EQ(p2.size(), 1);
    EXPECT_EQ(p2.active_entries(), 1);

    // ptr 指向的条目仍有效
    EXPECT_EQ(ptr.length(), 5);
}

TEST(Utf8StringPoolTest, RefCountAutoRelease) {
    Utf8StringPool pool;
    {
        auto p = pool.intern("Temp");
        EXPECT_EQ(pool.active_entries(), 1);
        EXPECT_EQ(pool.total_bytes(), 4);
    }
    // 离开作用域，refCount=0，数据释放
    EXPECT_EQ(pool.active_entries(), 0);
    EXPECT_EQ(pool.total_bytes(), 0);
}

TEST(Utf8StringPoolTest, PooledPtrCompareWithRef) {
    Utf8StringPool pool;
    auto p = pool.intern("Hello");

    u8 data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    Utf8StringRef ref(data, 5, 5);

    EXPECT_TRUE(p == ref);
    EXPECT_FALSE(p != ref);
}

TEST(Utf8StringPoolTest, LargeBatch) {
    Utf8StringPool pool;
    Utf8StringPooledPtr kept;
    for (int i = 0; i < 1000; ++i) {
        auto p = pool.intern("FixedKey");
        EXPECT_TRUE(p);
        if (i == 0) kept = p;  // 保持一个引用不被析构
    }
    EXPECT_EQ(pool.size(), 1);      // 只有 1 个唯一条目
    EXPECT_EQ(pool.active_entries(), 1);
}

// ---- Step 1: PooledPtr 隐式转 Utf8StringRef（读货币降级） ----

namespace {
static usize takesRef(Utf8StringRef r) { return r.byte_length(); }
}

TEST(Utf8StringPooledPtrTest, ImplicitConvertToRef) {
    Utf8StringPool pool;
    auto p = pool.intern("Hello");
    Utf8StringRef r = p;                    // 隐式转换，无需 .ref()
    EXPECT_EQ(r.byte_length(), 5u);
    EXPECT_EQ(r.data(), p.data());          // 同一字节
    EXPECT_EQ(takesRef(p), 5u);             // 直接作实参
    EXPECT_TRUE(r.equals("Hello"));
}

TEST(Utf8StringPooledPtrTest, ImplicitConvertEmpty) {
    Utf8StringPooledPtr p;
    Utf8StringRef r = p;
    EXPECT_TRUE(r.is_empty());
}

}  // namespace ca::str
