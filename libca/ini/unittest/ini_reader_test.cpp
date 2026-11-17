#include "libca/ini/ini_reader.hpp"
#include "ini_test_support.hpp"

#include <gtest/gtest.h>

using namespace ca::ini;
using namespace ca::ini::test;

TEST(IniReaderTest, ReadsSectionsAndKeys) {
    auto document = read_ok(
        "root = yes\n"
        "[server]\n"
        "host = 127.0.0.1\n"
        "port: 8080\n");
    ASSERT_TRUE(document.has(ref(""), ref("root")));
    ASSERT_TRUE(document.has_section(ref("server")));
    EXPECT_EQ(get_ok(document, "server", "host").to_std_string(), "127.0.0.1");
    EXPECT_EQ(get_ok(document, "server", "port").to_std_string(), "8080");
}

TEST(IniReaderTest, EscapedQuotesDoNotStartInlineComment) {
    auto document = read_ok(
        "[quote]\n"
        "value = \"a \\\" # not comment\" # comment\n"
        "single = 'a \\' ; not comment' ; comment\n");

    EXPECT_EQ(get_ok(document, "quote", "value").to_std_string(), "\"a \\\" # not comment\"");
    EXPECT_EQ(get_ok(document, "quote", "single").to_std_string(), "'a \\' ; not comment'");
}

TEST(IniReaderTest, DefaultKeepsLastOnDuplicateSection) {
    // 默认 KeepLast：同名 section 合并到索引最后一个。
    auto document = read_ok(
        "[s]\nx = 1\n"
        "[s]\ny = 2\n");
    EXPECT_EQ(get_ok(document, "s", "y").to_std_string(), "2");
}

TEST(IniReaderTest, DefaultKeepsLastOnDuplicateKey) {
    auto document = read_ok("[s]\nx = first\nx = second\n");
    EXPECT_EQ(get_ok(document, "s", "x").to_std_string(), "second");
}

TEST(IniReaderTest, HandlesUtf8KeysAndValues) {
    auto document = read_ok("[\xE6\x9C\x8D\xE5\x8A\xA1]\n"
                            "\xE5\x9C\xB0\xE5\x9D\x80 = \xE5\x8C\x97\xE4\xBA\xAC\n");
    // section "服务"，key "地址"，value "北京"
    ASSERT_TRUE(document.has_section(ref("\xE6\x9C\x8D\xE5\x8A\xA1")));
    EXPECT_EQ(get_ok(document, "\xE6\x9C\x8D\xE5\x8A\xA1", "\xE5\x9C\xB0\xE5\x9D\x80").to_std_string(),
              "\xE5\x8C\x97\xE4\xBA\xAC");
}

// UTF-8 BOM（Windows 记事本默认）不再污染首个 key/section。
TEST(IniReaderTest, BomIsSkipped) {
    const char bom_input[] = "\xEF\xBB\xBF[s]\nkey = v\n";
    auto doc = read_ok(bom_input);
    EXPECT_EQ(get_ok(doc, "s", "key").to_std_string(), "v");

    const char bom_kv[] = "\xEF\xBB\xBFkey = v\n";
    auto doc2 = read_ok(bom_kv);
    EXPECT_EQ(get_ok(doc2, "", "key").to_std_string(), "v");
}

TEST(IniReaderTest, AllowsDisablingEachInlineCommentMarker) {
    IniReaderOptions hash_disabled;
    hash_disabled.hash_comment = false;
    const auto with_hash = read_ok("key=value # kept ; removed\n", hash_disabled);
    EXPECT_EQ(get_ok(with_hash, "", "key").to_std_string(), "value # kept");

    IniReaderOptions semicolon_disabled;
    semicolon_disabled.semicolon_comment = false;
    const auto with_semicolon = read_ok("key=value ; kept # removed\n", semicolon_disabled);
    EXPECT_EQ(get_ok(with_semicolon, "", "key").to_std_string(), "value ; kept");
}

TEST(IniReaderTest, InlineCommentWhitespaceOptionControlsAdjacentMarker) {
    const auto strict = read_ok("key=value#tail\n");
    EXPECT_EQ(get_ok(strict, "", "key").to_std_string(), "value#tail");

    IniReaderOptions options;
    options.inline_comment_strict_whitespace = false;
    const auto relaxed = read_ok("key=value#tail\n", options);
    EXPECT_EQ(get_ok(relaxed, "", "key").to_std_string(), "value");
}
