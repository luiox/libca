#include <gmock/gmock.h>

#include <libca/core/status.hpp>
#include <libca/str/charset.hpp>

#include <string>

namespace ca::str {

// 验证用例选取 "中文" 这两个汉字的已知编码（码点断言跨平台成立：
// wstring 在 Windows 为 UTF-16、Linux 为 UCS-4，码点值相同）：
//   UTF-8 : E4 B8 AD E6 96 87（6 字节）
//   GBK   : D6 D0 CE C4（4 字节）
//   码点   : U+4E2D U+6587（2 个 wchar_t）
//
// GBK 用例在 POSIX 上依赖 iconv 的 GBK 转换器（glibc 自带；裁剪系统可能缺），
// 返回 UNIMPLEMENTED 时跳过而非报错。

TEST(CharsetConverterTest, Utf8ToWideRoundtrip) {
    const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87";  // "中文"
    auto wide = CharsetConverter::utf8_to_wide(utf8);
    ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
    const std::wstring expected{0x4E2D, 0x6587};
    EXPECT_EQ(wide.unwrap(), expected);

    auto back = CharsetConverter::wide_to_utf8(expected);
    ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
    EXPECT_EQ(back.unwrap(), utf8);
}

TEST(CharsetConverterTest, GbkToUtf8) {
    const std::string gbk = "\xD6\xD0\xCE\xC4";  // "中文" in GBK
    auto utf8 = CharsetConverter::gbk_to_utf8(gbk);
    if (utf8.is_err() && utf8.unwrap_err().code() == core::StatusCode::UNIMPLEMENTED)
        GTEST_SKIP() << "iconv lacks the GBK converter on this system";
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    EXPECT_EQ(utf8.unwrap(), std::string("\xE4\xB8\xAD\xE6\x96\x87"));
}

TEST(CharsetConverterTest, Utf8ToGbk) {
    const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87";  // "中文"
    auto gbk = CharsetConverter::utf8_to_gbk(utf8);
    if (gbk.is_err() && gbk.unwrap_err().code() == core::StatusCode::UNIMPLEMENTED)
        GTEST_SKIP() << "iconv lacks the GBK converter on this system";
    ASSERT_TRUE(gbk.is_ok()) << gbk.unwrap_err().to_string();
    EXPECT_EQ(gbk.unwrap(), std::string("\xD6\xD0\xCE\xC4"));
}

TEST(CharsetConverterTest, GbkRoundtrip) {
    const std::string gbk = "\xD6\xD0\xCE\xC4";  // "中文"
    auto utf8 = CharsetConverter::gbk_to_utf8(gbk);
    if (utf8.is_err() && utf8.unwrap_err().code() == core::StatusCode::UNIMPLEMENTED)
        GTEST_SKIP() << "iconv lacks the GBK converter on this system";
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    auto back = CharsetConverter::utf8_to_gbk(utf8.unwrap());
    ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
    EXPECT_EQ(back.unwrap(), gbk);
}

TEST(CharsetConverterTest, GbkWideRoundtrip) {
    const std::string gbk = "\xD6\xD0\xCE\xC4";  // "中文"
    auto wide = CharsetConverter::gbk_to_wide(gbk);
    if (wide.is_err() && wide.unwrap_err().code() == core::StatusCode::UNIMPLEMENTED)
        GTEST_SKIP() << "iconv lacks the GBK converter on this system";
    ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
    const std::wstring expected{0x4E2D, 0x6587};
    EXPECT_EQ(wide.unwrap(), expected);

    auto back = CharsetConverter::wide_to_gbk(expected);
    ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
    EXPECT_EQ(back.unwrap(), gbk);
}

TEST(CharsetConverterTest, EmptyInputIsOk) {
    auto wide = CharsetConverter::utf8_to_wide("");
    ASSERT_TRUE(wide.is_ok());
    EXPECT_TRUE(wide.unwrap().empty());

    auto utf8 = CharsetConverter::gbk_to_utf8("");
    ASSERT_TRUE(utf8.is_ok());
    EXPECT_TRUE(utf8.unwrap().empty());
}

TEST(CharsetConverterTest, InvalidUtf8Rejected) {
    // 0xFF 是非法 UTF-8 起始字节。
    auto wide = CharsetConverter::utf8_to_wide("\xFF");
    EXPECT_TRUE(wide.is_err());
    EXPECT_EQ(wide.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}

// ASCII 在任何本地代码页下（CP_ACP / locale codeset）不变，断言与系统区域无关。
TEST(CharsetConverterTest, LocalAsciiPassthrough) {
    auto utf8 = CharsetConverter::local_to_utf8("hello");
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    EXPECT_EQ(utf8.unwrap(), "hello");

    auto wide = CharsetConverter::local_to_wide("hi");
    ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
    EXPECT_EQ(wide.unwrap(), std::wstring(L"hi"));
}

#if defined(_WIN32)

// UTF-16 孤立代理项是非法序列（Windows wchar 为 UTF-16 才能表达）。
TEST(CharsetConverterTest, InvalidUtf16Rejected) {
    const std::wstring invalid{static_cast<wchar_t>(0xD800)};
    auto utf8 = CharsetConverter::wide_to_utf8(invalid);
    ASSERT_TRUE(utf8.is_err());
    EXPECT_EQ(utf8.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}

#else  // !defined(_WIN32)

// POSIX wchar 为 UCS-4：超出 Unicode 上限的码点是非法序列。
TEST(CharsetConverterTest, InvalidWideCodePointRejected) {
    const std::wstring invalid{static_cast<wchar_t>(0x110000)};
    auto utf8 = CharsetConverter::wide_to_utf8(invalid);
    ASSERT_TRUE(utf8.is_err());
    EXPECT_EQ(utf8.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}

#endif  // defined(_WIN32)

}  // namespace ca::str
