#include <gmock/gmock.h>

#include <libca/core/status.hpp>
#include <libca/str/charset.hpp>

#include <string>

namespace ca::str {

#if defined(_WIN32)

// Windows 上 CharsetConverter 应能正确完成 UTF-8 ↔ wchar ↔ GBK 三组转换。
// 验证用例选取 "中文" 这两个汉字的已知编码：
//   UTF-8 : E4 B8 AD E6 96 87（6 字节）
//   GBK   : D6 D0 CE C4（4 字节）
//   wchar : 4E2D 6587（2 个 wchar_t）

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
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    EXPECT_EQ(utf8.unwrap(), std::string("\xE4\xB8\xAD\xE6\x96\x87"));
}

TEST(CharsetConverterTest, Utf8ToGbk) {
    const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87";  // "中文"
    auto gbk = CharsetConverter::utf8_to_gbk(utf8);
    ASSERT_TRUE(gbk.is_ok()) << gbk.unwrap_err().to_string();
    EXPECT_EQ(gbk.unwrap(), std::string("\xD6\xD0\xCE\xC4"));
}

TEST(CharsetConverterTest, GbkRoundtrip) {
    const std::string gbk = "\xD6\xD0\xCE\xC4";  // "中文"
    auto utf8 = CharsetConverter::gbk_to_utf8(gbk);
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    auto back = CharsetConverter::utf8_to_gbk(utf8.unwrap());
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
    EXPECT_NE(wide.unwrap_err().code(), core::StatusCode::OK);
}

#else  // !defined(_WIN32)

// 非 Windows 平台：所有方法应统一返回 UNIMPLEMENTED，便于跨平台代码引用同一签名。
TEST(CharsetConverterTest, UnimplementedOnNonWindows) {
    auto wide  = CharsetConverter::utf8_to_wide("x");
    auto utf8  = CharsetConverter::wide_to_utf8(L"x");
    auto lw    = CharsetConverter::local_to_wide("x");
    auto lu    = CharsetConverter::local_to_utf8("x");
    auto g2u   = CharsetConverter::gbk_to_utf8("x");
    auto u2g   = CharsetConverter::utf8_to_gbk("x");
    auto g2w   = CharsetConverter::gbk_to_wide("x");
    auto w2g   = CharsetConverter::wide_to_gbk(L"x");

    EXPECT_FALSE(wide.is_ok());
    EXPECT_EQ(wide.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
    EXPECT_FALSE(utf8.is_ok());
    EXPECT_EQ(utf8.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
    EXPECT_FALSE(lw.is_ok());
    EXPECT_EQ(lw.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
    EXPECT_FALSE(lu.is_ok());
    EXPECT_EQ(lu.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
    EXPECT_FALSE(g2u.is_ok());
    EXPECT_EQ(g2u.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
    EXPECT_FALSE(u2g.is_ok());
    EXPECT_EQ(u2g.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
    EXPECT_FALSE(g2w.is_ok());
    EXPECT_EQ(g2w.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
    EXPECT_FALSE(w2g.is_ok());
    EXPECT_EQ(w2g.unwrap_err().code(), core::StatusCode::UNIMPLEMENTED);
}

#endif  // defined(_WIN32)

}  // namespace ca::str
