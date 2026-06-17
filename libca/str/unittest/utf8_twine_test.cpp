#include <gtest/gtest.h>
#include <libca/str/utf8_twine.hpp>
#include <libca/str/utf8_string_arena.hpp>
#include <libca/str/utf8_string_pool.hpp>

namespace ca::str {

TEST(Utf8TwineTest, SingleLeafToString) {
    Utf8Twine t("Hello");
    auto s = t.to_string();
    EXPECT_TRUE(s.equals("Hello"));
    EXPECT_EQ(t.byte_length(), 5u);
}

TEST(Utf8TwineTest, ConcatTwo) {
    auto s = (Utf8Twine("foo") + "bar").to_string();
    EXPECT_TRUE(s.equals("foobar"));
}

TEST(Utf8TwineTest, ConcatThreeWithDelimiter) {
    Utf8String a = Utf8String::from_cstr("java/lang");
    Utf8String b = Utf8String::from_cstr("Object");
    auto s = (Utf8Twine(a) + "/" + b).to_string();
    EXPECT_TRUE(s.equals("java/lang/Object"));
    EXPECT_EQ(s.byte_length(), 16u);
}

TEST(Utf8TwineTest, EmptyFragments) {
    auto s = (Utf8Twine("") + "x" + "").to_string();
    EXPECT_TRUE(s.equals("x"));
    EXPECT_TRUE(Utf8Twine().is_empty());
}

TEST(Utf8TwineTest, MaterializeIntoArena) {
    Utf8StringArena arena;
    Utf8StringRef r = (Utf8Twine("a") + "/" + "b").materialize(arena);
    EXPECT_TRUE(r.equals("a/b"));
    // 再 materialize 同内容 → arena 去重，同指针
    Utf8StringRef r2 = (Utf8Twine("a") + "/" + "b").materialize(arena);
    EXPECT_EQ(r.data(), r2.data());
}

TEST(Utf8TwineTest, MaterializeIntoPool) {
    Utf8StringPool pool;
    auto p = (Utf8Twine("Ljava/lang/") + "String;").materialize(pool);
    EXPECT_TRUE(p.ref().equals("Ljava/lang/String;"));
    EXPECT_EQ(pool.active_entries(), 1u);
}

TEST(Utf8TwineTest, SingleLeafMaterializeFastPath) {
    // 单叶子无右子：直接 intern 视图（不走 builder）
    Utf8StringArena arena;
    Utf8StringRef r = Utf8Twine("solo").materialize(arena);
    EXPECT_TRUE(r.equals("solo"));
}

TEST(Utf8TwineTest, PooledPtrAsFragment) {
    // PooledPtr 经隐式转 Ref 喂进 Twine（验证读货币贯通）
    Utf8StringPool pool;
    auto owner = pool.intern("pkg");
    auto s = (Utf8Twine(owner.ref()) + "/Cls").to_string();
    EXPECT_TRUE(s.equals("pkg/Cls"));
}

}  // namespace ca::str
