#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "libca/str/os_string.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::str::test {
namespace {

// ASCII 在所有平台都应正确往返。
TEST(OsStringTest, AsciiRoundtrip)
{
    OsString os = OsString::from_utf8("hello world");
    EXPECT_FALSE(os.is_empty());

    Utf8String back = os.to_utf8_lossy();
    EXPECT_EQ(static_cast<std::string_view>(back), "hello world");
}

TEST(OsStringTest, EmptyString)
{
    OsString os;
    EXPECT_TRUE(os.is_empty());

    OsString from_empty = OsString::from_utf8("");
    EXPECT_TRUE(from_empty.is_empty());
    EXPECT_EQ(static_cast<std::string_view>(from_empty.to_utf8_lossy()), "");
}

#if defined(_WIN32)

// 验收标准：Windows 上 OsString ↔ std::wstring 零拷贝互转。
TEST(OsStringTest, WideStringZeroCopyInterop)
{
    std::wstring original = L"path/to/file";
    OsString     os       = OsString::from_wstring(std::wstring(original));

    // as_wide 返回的视图应与原数据一致。
    std::wstring_view view = os.as_wide();
    EXPECT_EQ(view, original);

    // into_wstring 取回所有权。
    std::wstring taken = os.into_wstring();
    EXPECT_EQ(taken, original);
}

// 验收标准：Windows 上含中文往返无损。
TEST(OsStringTest, ChineseRoundtripViaUtf16)
{
    const char*    utf8_in = "你好世界";  // NOLINT: 故意使用 UTF-8 字面量
    std::u16string expected = u"你好世界";

    OsString os = OsString::from_utf8(utf8_in);

    auto wide = os.as_wide();
    ASSERT_EQ(wide.size(), expected.size());
    for (std::size_t i = 0; i < wide.size(); ++i)
        EXPECT_EQ(wide[i], static_cast<wchar_t>(expected[i]));

    Utf8String back = os.to_utf8_lossy();
    EXPECT_EQ(static_cast<std::string_view>(back), std::string_view(utf8_in));
}

TEST(OsStringTest, FromUtf8LossyReplacesInvalid)
{
    // 非法 UTF-8 序列（孤立的续字节），不应抛异常。
    EXPECT_NO_THROW({
        OsString os = OsString::from_utf8_lossy(std::string_view("\xff\xfe", 2));
    });
}

TEST(OsStringTest, FromUtf8StrictThrowsOnInvalid)
{
    EXPECT_THROW({ OsString::from_utf8(std::string_view("\xff\xfe", 2)); },
                 std::runtime_error);
}

#else  // POSIX

// 验收标准：POSIX 上 OsString ↔ Utf8String 零拷贝互转。
TEST(OsStringTest, Utf8ZeroCopyInterop)
{
    Utf8String original = Utf8String::from_cstr("hello");
    const u8*  ptr_before = original.data();

    OsString os = OsString::from_utf8_string(std::move(original));

    // as_utf8 零拷贝访问。
    std::string_view view = os.as_utf8();
    EXPECT_EQ(view, "hello");

    // into_utf8_string move 出来，指针应保持有效（零拷贝）。
    Utf8String taken = os.into_utf8_string();
    EXPECT_EQ(taken.data(), ptr_before);
    EXPECT_EQ(static_cast<std::string_view>(taken), "hello");
}

TEST(OsStringTest, ChineseRoundtripNativeUtf8)
{
    const char* utf8_in = "你好世界";  // NOLINT
    OsString    os      = OsString::from_utf8(utf8_in);

    // POSIX 下 as_utf8 直接返回内部 UTF-8，无转换开销。
    EXPECT_EQ(os.as_utf8(), std::string_view(utf8_in));
}

#endif

TEST(OsStringTest, MoveSemantics)
{
    OsString a = OsString::from_utf8("content");
    OsString b = std::move(a);

    EXPECT_TRUE(a.is_empty());   // NOLINT: moved-from 状态
    EXPECT_FALSE(b.is_empty());
}

TEST(OsStrTest, ViewReflectsSource)
{
    OsString os  = OsString::from_utf8("test");
    OsStr    view = os.as_view();

    EXPECT_FALSE(view.is_empty());
    EXPECT_GT(view.size(), 0u);
}

}  // namespace
}  // namespace ca::str::test
