#include <gtest/gtest.h>

#include <string_view>

#include "libca/str/bom.hpp"

// 测试里大量字节串含 \x00，std::string_view(const char*) 会按 strlen 截断，
// 统一用 ""sv 字面量（按数组尺寸构造，嵌入 0 字节安全）。
using namespace std::string_view_literals;

namespace ca::str {

// ============================================================================
// detect_bom：各类型命中
// ============================================================================

TEST(BomTest, DetectsUtf8)
{
    EXPECT_EQ(detect_bom("\xEF\xBB\xBFhello"sv), BomType::Utf8);
    // 恰好只有完整 BOM
    EXPECT_EQ(detect_bom("\xEF\xBB\xBF"sv), BomType::Utf8);
}

TEST(BomTest, DetectsUtf16Le)
{
    EXPECT_EQ(detect_bom("\xFF\xFEh\x00i\x00"sv), BomType::Utf16Le);
    EXPECT_EQ(detect_bom("\xFF\xFE"sv), BomType::Utf16Le);
}

TEST(BomTest, DetectsUtf16Be)
{
    EXPECT_EQ(detect_bom("\xFE\xFF\x00h\x00i"sv), BomType::Utf16Be);
    EXPECT_EQ(detect_bom("\xFE\xFF"sv), BomType::Utf16Be);
}

TEST(BomTest, DetectsUtf32Le)
{
    EXPECT_EQ(detect_bom("\xFF\xFE\x00\x00h\x00\x00\x00"sv), BomType::Utf32Le);
    EXPECT_EQ(detect_bom("\xFF\xFE\x00\x00"sv), BomType::Utf32Le);
}

TEST(BomTest, DetectsUtf32Be)
{
    EXPECT_EQ(detect_bom("\x00\x00\xFE\xFF\x00\x00\x00h"sv), BomType::Utf32Be);
    EXPECT_EQ(detect_bom("\x00\x00\xFE\xFF"sv), BomType::Utf32Be);
}

// ============================================================================
// detect_bom：前缀重叠判别与长度不足不误判
// ============================================================================

TEST(BomTest, Utf32LeWinsOverUtf16LePrefix)
{
    // FF FE 00 00 与 FF FE 前缀重叠，必须判为更长的 UTF-32LE。
    EXPECT_EQ(detect_bom("\xFF\xFE\x00\x00"sv), BomType::Utf32Le);
}

TEST(BomTest, TruncatedBomDoesNotOverMatch)
{
    // FF FE 00 只有 3 字节：不能确认 UTF-32LE 的完整 4 字节序列，判回 UTF-16LE。
    EXPECT_EQ(detect_bom("\xFF\xFE\x00"sv), BomType::Utf16Le);
    // EF BB 只有 2 字节：不足完整 UTF-8 BOM，返回 None。
    EXPECT_EQ(detect_bom("\xEF\xBB"sv), BomType::None);
    // 00 00 FE 只有 3 字节：不足完整 UTF-32BE BOM，返回 None。
    EXPECT_EQ(detect_bom("\x00\x00\xFE"sv), BomType::None);
    // 单字节 FF/FE/EF 都不构成任何 BOM。
    EXPECT_EQ(detect_bom("\xFF"sv), BomType::None);
    EXPECT_EQ(detect_bom("\xFE"sv), BomType::None);
    EXPECT_EQ(detect_bom("\xEF"sv), BomType::None);
}

TEST(BomTest, NoBomCases)
{
    EXPECT_EQ(detect_bom(""sv), BomType::None);
    EXPECT_EQ(detect_bom("plain ascii"sv), BomType::None);
    EXPECT_EQ(detect_bom("{\"key\": 1}"sv), BomType::None);
    // BOM 之后的内容不影响判型；UTF-8 BOM 不与其它 BOM 前缀重叠。
    EXPECT_EQ(detect_bom("\xEF\xBB\xBF\xEF\xBB"sv), BomType::Utf8);
}

// ============================================================================
// bom_byte_length
// ============================================================================

TEST(BomTest, ByteLengthPerType)
{
    EXPECT_EQ(bom_byte_length(BomType::None), 0u);
    EXPECT_EQ(bom_byte_length(BomType::Utf8), 3u);
    EXPECT_EQ(bom_byte_length(BomType::Utf16Le), 2u);
    EXPECT_EQ(bom_byte_length(BomType::Utf16Be), 2u);
    EXPECT_EQ(bom_byte_length(BomType::Utf32Le), 4u);
    EXPECT_EQ(bom_byte_length(BomType::Utf32Be), 4u);
}

// ============================================================================
// strip_bom
// ============================================================================

TEST(BomTest, StripRemovesBomOnly)
{
    EXPECT_EQ(strip_bom("\xEF\xBB\xBFhello"sv), "hello"sv);
    EXPECT_EQ(strip_bom("\xFF\xFEh\x00i\x00"sv), "h\x00i\x00"sv);
    EXPECT_EQ(strip_bom("\xFE\xFF\x00h\x00i"sv), "\x00h\x00i"sv);
    EXPECT_EQ(strip_bom("\x00\x00\xFE\xFF\x00\x00\x00h"sv), "\x00\x00\x00h"sv);
}

TEST(BomTest, StripWithoutBomReturnsInputUnchanged)
{
    std::string_view input = "no bom here";
    std::string_view stripped = strip_bom(input);
    EXPECT_EQ(stripped, input);
    EXPECT_EQ(stripped.data(), input.data());  // 零拷贝：同一底层指针
    EXPECT_EQ(strip_bom(""sv), ""sv);
    EXPECT_EQ(strip_bom("\xEF\xBB"sv), "\xEF\xBB"sv);  // 截断 BOM 原样保留
}

TEST(BomTest, StripOnlyBomYieldsEmpty)
{
    EXPECT_EQ(strip_bom("\xEF\xBB\xBF"sv), ""sv);
    EXPECT_EQ(strip_bom("\xFF\xFE\x00\x00"sv), ""sv);  // UTF-32LE 全剥离
}

}  // namespace ca::str
