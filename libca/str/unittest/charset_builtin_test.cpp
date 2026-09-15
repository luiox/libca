//
// @brief 编码内置化固定向量测试：GB18030（含 GBK）/ Latin-1 / Windows-1252 /
//        supported / list_supported 的跨平台确定行为。
// @author Canrad
// @date 2026/09/15
//
// 向量来源：python 3.x 内置 codec gb18030 / cp1252 对拍后硬编码（hex），生成
// 环境与断言脚本见 tools/gen_charset_tables.py 的头部说明；涉及 WHATWG 已发布
// 值的向量在用例注释中标注。内置路径下所有编码行为不再依赖 gconv / 系统代码页，
// 本文件所有断言跨平台成立（Windows MSVC / Linux glibc / MinGW）。
//

#include <gmock/gmock.h>

#include <libca/core/status.hpp>
#include <libca/str/charset.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace ca::str {

// ============================================================================
// GB18030 / GBK 内置表驱动
// ============================================================================

// 固定向量（python gb18030 codec 对拍；8140→U+4E02 亦是 WHATWG index-gb18030[0]）。
TEST(Gb18030BuiltinTest, DecodeKnownVectors) {
    struct Case {
        const char*    bytes;
        usize          size;
        const wchar_t* expect_wide;
        usize          expect_len;
    };
    const Case cases[] = {
        {"\x81\x40", 2, L"\x4E02", 1},              // 丂（双字节表首项）
        {"\xA1\xA1", 2, L"\x3000", 1},              // 全角空格（GB2312 区）
        {"\xD6\xD0\xCE\xC4", 4, L"\x4E2D\x6587", 2}, // 中文
        {"\xAA\xA1", 2, L"\xE000", 1},              // PUA 区段起点
        {"\xA7\xA0", 2, L"\xE765", 1},              // PUA 区段末段
        {"\xA2\xE3", 2, L"\x20AC", 1},              // 欧元（GB18030 编码）
    };
    for (const auto& c : cases) {
        auto utf8 = CharsetConverter::gbk_to_utf8(std::string_view(c.bytes, c.size));
        ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
        auto wide = CharsetConverter::gbk_to_wide(std::string_view(c.bytes, c.size));
        ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
        const std::wstring expected(c.expect_wide, c.expect_wide + c.expect_len);
        EXPECT_EQ(wide.unwrap(), expected) << "input " << c.bytes;
    }
}

// GB18030 四字节序列（线性区间表；向量来自 python gb18030 codec）。
// 经 utf8_to_wide 构造 wide 输入：Windows wchar 为 UTF-16（代理对）、POSIX 为
// UCS-4，跨平台成立。
TEST(Gb18030BuiltinTest, FourByteSequences) {
    // {码点 UTF-8, 四字节编码}；U+0080 与 U+FFFF 也走四字节（不在双字节表）。
    const std::pair<const char*, const char*> cases[] = {
        {"\xC2\x80", "\x81\x30\x81\x30"},         // U+0080
        {"\xF0\xA0\x80\x80", "\x95\x32\x82\x36"}, // U+20000
        {"\xF0\x9F\x98\x80", "\x94\x39\xFC\x36"}, // U+1F600
        {"\xEF\xBF\xBF", "\x84\x31\xA4\x39"},     // U+FFFF
        {"\xF4\x8F\xBF\xBF", "\xE3\x32\x9A\x35"}, // U+10FFFF
    };
    for (const auto& c : cases) {
        auto wide = CharsetConverter::utf8_to_wide(c.first);
        ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
        auto gb = CharsetConverter::wide_to_gb18030(wide.unwrap());
        ASSERT_TRUE(gb.is_ok()) << gb.unwrap_err().to_string();
        EXPECT_EQ(gb.unwrap(), std::string(c.second, 4)) << "encode " << c.first;

        auto back = CharsetConverter::gb18030_to_wide(std::string_view(c.second, 4));
        ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
        auto back_utf8 = CharsetConverter::wide_to_utf8(back.unwrap());
        ASSERT_TRUE(back_utf8.is_ok()) << back_utf8.unwrap_err().to_string();
        EXPECT_EQ(back_utf8.unwrap(), std::string(c.first));
    }
}

// 混合样本往返：中文标点 + ASCII（python gb18030 codec 对拍）。
TEST(Gb18030BuiltinTest, MixedSampleRoundtrip) {
    const std::string utf8 =
        "\xE4\xB8\xAD\xE5\x8D\x8E\xE4\xBA\xBA\xE6\xB0\x91\xE5\x85\xB1\xE5\x92\x8C"
        "\xE5\x9B\xBD\xEF\xBC\x8C\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x81\x48\x65\x6C"
        "\x6C\x6F\x20\x31\x32\x33";  // 中华人民共和国，你好！Hello 123
    const std::string gb =
        "\xD6\xD0\xBB\xAA\xC8\xCB\xC3\xF1\xB9\xB2\xBA\xCD\xB9\xFA\xA3\xAC\xC4\xE3"
        "\xBA\xC3\xA3\xA1\x48\x65\x6C\x6C\x6F\x20\x31\x32\x33";

    auto gbk = CharsetConverter::utf8_to_gbk(utf8);
    ASSERT_TRUE(gbk.is_ok()) << gbk.unwrap_err().to_string();
    EXPECT_EQ(gbk.unwrap(), gb);

    auto back = CharsetConverter::gbk_to_utf8(gb);
    ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
    EXPECT_EQ(back.unwrap(), utf8);
}

// ASCII 直通。
TEST(Gb18030BuiltinTest, AsciiPassthrough) {
    auto gb = CharsetConverter::utf8_to_gb18030("Hello 123");
    ASSERT_TRUE(gb.is_ok()) << gb.unwrap_err().to_string();
    EXPECT_EQ(gb.unwrap(), "Hello 123");
    auto utf8 = CharsetConverter::gb18030_to_utf8("Hello 123");
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    EXPECT_EQ(utf8.unwrap(), "Hello 123");
}

// 与 CP_936 的钉死差异：0x80 在 GB18030 中是非法字节（CP_936 把它映射为欧元），
// 内置路径按 GB18030 报错；欧元的 GB18030 编码是双字节 A2 E3（CP_936 是 0x80）。
TEST(Gb18030BuiltinTest, Cp936EuroDifferencePinned) {
    auto bad = CharsetConverter::gbk_to_utf8("\x80");
    ASSERT_TRUE(bad.is_err());
    EXPECT_EQ(bad.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);

    auto euro = CharsetConverter::utf8_to_gbk("\xE2\x82\xAC");  // €
    ASSERT_TRUE(euro.is_ok()) << euro.unwrap_err().to_string();
    EXPECT_EQ(euro.unwrap(), std::string("\xA2\xE3"));
}

// 非法 / 残缺序列一律 Err（INVALID_ARGUMENT），不做静默字节替换。
TEST(Gb18030BuiltinTest, InvalidSequencesRejected) {
    const std::string invalid_inputs[] = {
        "\x80",                   // 非法单字节（CP_936 欧元位，GB18030 拒绝）
        "\xFF",                   // 非法单字节
        "\xD6",                   // 双字节残缺
        "\xD6\x7F",               // 非法 trail（0x7F 不在 0x40-0x7E）
        "\xD6\x00",               // 非法 trail（0x00）
        "\xD6\x39",               // 0x39 进入四字节路径但长度不足
        "\x81\x30\x41\x30",       // 四字节第 3 字节非 0x81-0xFE
        "\x81\x30\x81\x40",       // 四字节第 4 字节非 0x30-0x39
        "\xFE\x39\xFE\x39",       // 指针 1587599 超出映射上限（GB18030 保留）
        "\x84\x31\xA5\x30",       // 指针 39420 落在保留指针洞 [39420, 188999]
    };
    for (const auto& input : invalid_inputs) {
        auto utf8 = CharsetConverter::gb18030_to_utf8(input);
        ASSERT_TRUE(utf8.is_err()) << "input " << input;
        EXPECT_EQ(utf8.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT)
            << "input " << input;
        EXPECT_FALSE(utf8.unwrap_err().message().empty()) << "input " << input;

        auto wide = CharsetConverter::gb18030_to_wide(input);
        ASSERT_TRUE(wide.is_err()) << "input " << input;
    }
}

// UTF-8 侧代理项输入（CESU-8 式 ED A0 80 = 代理项 U+D800）一律拒绝，全平台一致。
TEST(Gb18030BuiltinTest, SurrogateUtf8InputRejected) {
    auto gb = CharsetConverter::utf8_to_gb18030("\xED\xA0\x80");
    ASSERT_TRUE(gb.is_err());
    EXPECT_EQ(gb.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);

    auto wide = CharsetConverter::utf8_to_wide("\xED\xA0\x80");
    ASSERT_TRUE(wide.is_err());
    EXPECT_EQ(wide.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}

// wide 侧孤立代理项输入拒绝（Windows wchar 为 UTF-16 直接表达；POSIX 为 UCS-4
// 数值等同代理码点，内置校验口径一致）。
TEST(Gb18030BuiltinTest, LoneSurrogateWideRejected) {
    const std::wstring lone{static_cast<wchar_t>(0xD800)};
    auto gb = CharsetConverter::wide_to_gbk(lone);
    ASSERT_TRUE(gb.is_err());
    EXPECT_EQ(gb.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);

    auto utf8 = CharsetConverter::wide_to_utf8(lone);
    ASSERT_TRUE(utf8.is_err());
    EXPECT_EQ(utf8.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}

// gconv 差异钉死（commit 2d005cb 的历史问题）：glibc 的 iconv 按 UCS-4 上限
// 0x7FFFFFFF 收值，0x110000 曾被编成 5 字节序列而不报错。内置路径把 wide 严格
// 限制在 Unicode 标量值范围，行为确定地拒绝（POSIX 专属用例：Windows wchar
// 是 16 位，无法表达该码点）。
#if !defined(_WIN32)
TEST(Gb18030BuiltinTest, OverRangeWideCodePointRejected) {
    const std::wstring over{static_cast<wchar_t>(0x110000)};
    auto utf8 = CharsetConverter::wide_to_utf8(over);
    ASSERT_TRUE(utf8.is_err());
    EXPECT_EQ(utf8.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}
#endif

// gb18030_* 与 gbk_* 同路径（GBK 统一按 GB18030 处理）。
TEST(Gb18030BuiltinTest, GbkAndGb18030SamePath) {
    const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87";  // 中文
    auto via_gbk = CharsetConverter::utf8_to_gbk(utf8);
    auto via_gb = CharsetConverter::utf8_to_gb18030(utf8);
    ASSERT_TRUE(via_gbk.is_ok()) << via_gbk.unwrap_err().to_string();
    ASSERT_TRUE(via_gb.is_ok()) << via_gb.unwrap_err().to_string();
    EXPECT_EQ(via_gbk.unwrap(), via_gb.unwrap());
}

// ============================================================================
// Latin-1（ISO-8859-1）
// ============================================================================

TEST(Latin1BuiltinTest, AllBytesRoundtrip) {
    std::string all;
    for (int b = 0; b < 256; ++b)
        all.push_back(static_cast<char>(b));
    auto utf8 = CharsetConverter::latin1_to_utf8(all);
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    EXPECT_EQ(utf8.unwrap().size(), all.size() + 128);  // 0x80-0xFF 各占 2 字节
    auto back = CharsetConverter::utf8_to_latin1(utf8.unwrap());
    ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
    EXPECT_EQ(back.unwrap(), all);
}

TEST(Latin1BuiltinTest, WideRoundtripAndReject) {
    const std::string latin1 = "caf\xE9";  // café
    auto wide = CharsetConverter::latin1_to_wide(latin1);
    ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
    const std::wstring expected{L'c', L'a', L'f', 0xE9};
    EXPECT_EQ(wide.unwrap(), expected);
    auto back = CharsetConverter::wide_to_latin1(expected);
    ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
    EXPECT_EQ(back.unwrap(), latin1);

    // >U+00FF 不可表示，报错不替换。
    auto rej = CharsetConverter::wide_to_latin1(std::wstring{0x4E2D});
    ASSERT_TRUE(rej.is_err());
    EXPECT_EQ(rej.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}

// ============================================================================
// Windows-1252
// ============================================================================

TEST(Cp1252BuiltinTest, DecodeWhatWGDiffTable) {
    // 27 项差异代表值（WHATWG index-windows-1252；python cp1252 对拍）。
    struct Case {
        char         byte;
        wchar_t      cp;
    };
    const Case cases[] = {
        {'\x80', 0x20AC},  // € EURO SIGN
        {'\x82', 0x201A},  // ‚
        {'\x84', 0x201E},  // „
        {'\x85', 0x2026},  // …
        {'\x91', 0x2018},  // '
        {'\x92', 0x2019},  // '
        {'\x93', 0x201C},  // "
        {'\x94', 0x201D},  // "
        {'\x96', 0x2013},  // –
        {'\x97', 0x2014},  // —
        {'\x99', 0x2122},  // ™
        {'\x9C', 0x0153},  // œ
        {'\x9F', 0x0178},  // Ÿ
    };
    for (const auto& c : cases) {
        auto wide = CharsetConverter::cp1252_to_wide(std::string(1, c.byte));
        ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
        EXPECT_EQ(wide.unwrap(), std::wstring(1, c.cp))
            << "byte 0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c.byte));
        auto utf8 = CharsetConverter::cp1252_to_utf8(std::string(1, c.byte));
        ASSERT_TRUE(utf8.is_ok());
    }
}

// 5 个 WHATWG 未定义字节（0x81/0x8D/0x8F/0x90/0x9D）按 Latin-1 恒等映射为 C1
// control（同 Windows best-fit；与 WHATWG/python 的报错口径不同，取全映射双射）。
TEST(Cp1252BuiltinTest, UndefinedBytesC1Identity) {
    const char undefined[] = {'\x81', '\x8D', '\x8F', '\x90', '\x9D'};
    for (char b : undefined) {
        const auto expect_cp = static_cast<wchar_t>(static_cast<unsigned char>(b));
        auto wide = CharsetConverter::cp1252_to_wide(std::string(1, b));
        ASSERT_TRUE(wide.is_ok()) << wide.unwrap_err().to_string();
        EXPECT_EQ(wide.unwrap(), std::wstring(1, expect_cp));
        auto back = CharsetConverter::wide_to_cp1252(std::wstring(1, expect_cp));
        ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
        EXPECT_EQ(back.unwrap(), std::string(1, b));
    }
}

// 全 256 字节解码 → 编码恒回环（拉丁区直映射 + 差异表 + C1 恒等）。
TEST(Cp1252BuiltinTest, FullRangeRoundtrip) {
    std::string all;
    for (int b = 0; b < 256; ++b)
        all.push_back(static_cast<char>(b));
    auto utf8 = CharsetConverter::cp1252_to_utf8(all);
    ASSERT_TRUE(utf8.is_ok()) << utf8.unwrap_err().to_string();
    auto back = CharsetConverter::utf8_to_cp1252(utf8.unwrap());
    ASSERT_TRUE(back.is_ok()) << back.unwrap_err().to_string();
    EXPECT_EQ(back.unwrap(), all);
}

// 非差异表的 >U+00FF 码点不可表示（如 U+4E2D、U+0100）。
TEST(Cp1252BuiltinTest, UnrepresentableRejected) {
    for (const char* utf8 : {"\xE4\xB8\xAD", "\xC4\x80"}) {
        auto out = CharsetConverter::utf8_to_cp1252(utf8);
        ASSERT_TRUE(out.is_err()) << utf8;
        EXPECT_EQ(out.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
    }
}

// ============================================================================
// supported / list_supported
// ============================================================================

TEST(CharsetQueryTest, BuiltinNamesSupportedCaseInsensitive) {
    const char* names[] = {
        "utf-8", "UTF-8", "utf8", "gb18030", "GB18030", "gbk", "GBK", "gb2312",
        "cp936", "iso-8859-1", "LATIN1", "latin-1", "windows-1252", "CP1252",
        "local",
    };
    for (const char* name : names)
        EXPECT_TRUE(CharsetConverter::supported(name)) << name;
}

TEST(CharsetQueryTest, EmptyAndGarbageNotSupported) {
    EXPECT_FALSE(CharsetConverter::supported(""));
    // 全平台都应回答 false 的确定性非法名。
    EXPECT_FALSE(CharsetConverter::supported("\x01\x02 not-a-charset \x03"));
}

TEST(CharsetQueryTest, ListSupportedSortedAndConsistent) {
    const auto names = CharsetConverter::list_supported();
    ASSERT_FALSE(names.empty());
    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(names, sorted);

    // 关键规范名与别名必须在列表中，且列表内所有名字都被 supported() 接受。
    const char* required[] = {
        "utf-8", "utf8", "gb18030", "gbk", "cp936", "gb2312",
        "iso-8859-1", "latin1", "windows-1252", "cp1252", "local",
    };
    for (const char* name : required) {
        EXPECT_NE(std::find(names.begin(), names.end(), name), names.end()) << name;
    }
    for (const auto& name : names)
        EXPECT_TRUE(CharsetConverter::supported(name)) << name;
}

#if !defined(_WIN32) && !defined(LIBCA_STR_NO_ICONV)
// iconv 启用时：内置表之外的长尾编码按 iconv 探测回答（Linux glibc 自带 big5）。
TEST(CharsetQueryTest, LongTailViaIconvProbe) {
    EXPECT_TRUE(CharsetConverter::supported("big5"));
}
#endif

}  // namespace ca::str
