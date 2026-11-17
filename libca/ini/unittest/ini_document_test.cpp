#include "libca/ini/ini_document.hpp"
#include "libca/ini/ini_writer.hpp"
#include "ini_test_support.hpp"

#include <clocale>
#include <limits>
#include <string>

#include <gtest/gtest.h>

using namespace ca::ini;
using namespace ca::ini::test;

TEST(IniDocumentTest, SetPreservesInlineCommentAndOrder) {
    auto document = read_ok(
        "# comment\n"
        "[server]\n"
        "host = 127.0.0.1 ; keep inline\n"
        "port = 8080\n");

    document.set(ref("server"), ref("host"), ref("0.0.0.0"));
    document.set(ref("server"), ref("mode"), ref("dev"));

    EXPECT_EQ(IniWriter::write(document).to_std_string(),
              "# comment\n"
              "[server]\n"
              "host = 0.0.0.0 ; keep inline\n"
              "port = 8080\n"
              "mode = dev\n");
}

TEST(IniDocumentTest, UpdatingNewKeyPreservesInsertedSpacing) {
    auto document = read_ok("[server]\n");
    document.set(ref("server"), ref("mode"), ref("dev"));
    document.set(ref("server"), ref("mode"), ref("prod"));

    EXPECT_EQ(IniWriter::write(document).to_std_string(),
              "[server]\n"
              "mode = prod\n");
}

TEST(IniDocumentTest, RemoveKeyAndSection) {
    auto document = read_ok(
        "[a]\n"
        "x = 1\n"
        "[b]\n"
        "y = 2\n"
        "; b comment\n");

    EXPECT_TRUE(document.remove(ref("a"), ref("x")));
    EXPECT_TRUE(document.remove_section(ref("b")));

    EXPECT_EQ(IniWriter::write(document).to_std_string(), "[a]\n");
}

TEST(IniDocumentTest, RemoveSectionKeepsNextSectionLeadingComments) {
    auto document = read_ok(
        "[a]\n"
        "x = 1\n"
        "\n"
        "; b docs\n"
        "[b]\n"
        "y = 2\n");

    EXPECT_TRUE(document.remove_section(ref("a")));

    EXPECT_EQ(IniWriter::write(document).to_std_string(),
              "\n"
              "; b docs\n"
              "[b]\n"
              "y = 2\n");
}

TEST(IniDocumentTest, KeysReturnsUniqueNames) {
    auto document = read_ok(
        "[server]\n"
        "host = first\n"
        "port = 1\n"
        "host = second\n");

    auto keys = document.keys(ref("server"));

    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0].to_std_string(), "host");
    EXPECT_EQ(keys[1].to_std_string(), "port");
    EXPECT_EQ(get_ok(document, "server", "host").to_std_string(), "second");
}

TEST(IniDocumentTypedTest, GetInt) {
    auto doc = read_ok("[s]\nx = 42\ny = -7\n");
    EXPECT_EQ(doc.get_int(ref("s"), ref("x")).unwrap_or(0), 42);
    EXPECT_EQ(doc.get_int(ref("s"), ref("y")).unwrap_or(0), -7);
}

TEST(IniDocumentTypedTest, GetIntStripsQuotes) {
    auto doc = read_ok("[s]\nx = \"8080\"\n");
    // 原始 get 返回带引号的值
    EXPECT_EQ(get_ok(doc, "s", "x").to_std_string(), "\"8080\"");
    // get_int 自动剥引号
    EXPECT_EQ(doc.get_int(ref("s"), ref("x")).unwrap_or(-1), 8080);
}

TEST(IniDocumentTypedTest, GetIntFailsOnNonNumber) {
    auto doc = read_ok("[s]\nx = abc\n");
    EXPECT_TRUE(doc.get_int(ref("s"), ref("x")).is_err());
}

TEST(IniDocumentTypedTest, GetDouble) {
    auto doc = read_ok("[s]\nx = 3.14\n");
    EXPECT_DOUBLE_EQ(doc.get_double(ref("s"), ref("x")).unwrap_or(0.0), 3.14);
}

// strtod 曾宽容前导 '+'（"+1.5" 合法）；收敛到 from_chars 后显式保留该宽容。
TEST(IniDocumentTypedTest, GetDoubleAcceptsLeadingPlus) {
    auto doc = read_ok("[s]\nx = +1.5\n");
    EXPECT_DOUBLE_EQ(doc.get_double(ref("s"), ref("x")).unwrap_or(0.0), 1.5);
}

// strtod 曾把十六进制浮点（"0x1p2" = 8.0）当 double 接受；from_chars 通用格式
// 不含十六进制，收敛后按非法处理（非 INI 常规语义，钉死新口径）。
TEST(IniDocumentTypedTest, GetDoubleRejectsHexFloat) {
    auto doc = read_ok("[s]\nx = 0x1p2\n");
    EXPECT_TRUE(doc.get_double(ref("s"), ref("x")).is_err());
}

// LC_NUMERIC 毒化回归：strtod 时代宿主程序 setlocale 到逗号小数点区域后
// "3.14" 会被截断判非法；from_chars 与进程 locale 无关。找不到该类 locale 则 SKIP。
TEST(IniDocumentTypedTest, GetDoubleImmuneToLcNumericLocale) {
    struct LocaleGuard {
        ~LocaleGuard() { std::setlocale(LC_NUMERIC, "C"); }
    } guard;
    const char* candidates[] = {"de_DE.UTF-8", "de-DE", "de_DE", "German_Germany"};
    bool applied = false;
    for (const char* name : candidates) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr) {
            applied = true;
            break;
        }
    }
    if (!applied) GTEST_SKIP() << "no comma-decimal locale available";

    auto doc = read_ok("[s]\nx = 3.14\n");
    EXPECT_DOUBLE_EQ(doc.get_double(ref("s"), ref("x")).unwrap_or(0.0), 3.14);
}

TEST(IniDocumentTypedTest, GetBoolAcceptsCommonForms) {
    auto doc = read_ok("[s]\nt = true\nf = false\nyes = yes\nno = no\non = on\noff = off\n"
                       "one = 1\nzero = 0\nUPPER = TRUE\n");
    EXPECT_TRUE(doc.get_bool(ref("s"), ref("t")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(ref("s"), ref("f")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(ref("s"), ref("yes")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(ref("s"), ref("no")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(ref("s"), ref("on")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(ref("s"), ref("off")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(ref("s"), ref("one")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(ref("s"), ref("zero")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(ref("s"), ref("UPPER")).unwrap_or(false));
}

TEST(IniDocumentTypedTest, GetBoolFailsOnUnknown) {
    auto doc = read_ok("[s]\nx = maybe\n");
    EXPECT_TRUE(doc.get_bool(ref("s"), ref("x")).is_err());
}

TEST(IniDocumentTypedTest, GetOrReturnsDefault) {
    auto doc = read_ok("[s]\nx = 1\n");
    EXPECT_EQ(doc.get_or(ref("s"), ref("x"), ref("default")).to_std_string(), "1");
    EXPECT_EQ(doc.get_or(ref("s"), ref("missing"), ref("default")).to_std_string(), "default");
}

TEST(IniDocumentTest, SetPreservesQuotesAroundValue) {
    // 原始 value 带引号，set 新值后应保留引号风格。
    auto document = read_ok("[server]\nhost = \"127.0.0.1\"\n");
    EXPECT_EQ(get_ok(document, "server", "host").to_std_string(), "\"127.0.0.1\"");
    document.set(ref("server"), ref("host"), ref("0.0.0.0"));
    EXPECT_EQ(IniWriter::write(document).to_std_string(),
              "[server]\n"
              "host = \"0.0.0.0\"\n");
}

TEST(IniDocumentTest, SetPreservesSingleQuotesAroundValue) {
    auto document = read_ok("[server]\nhost = 'localhost'\n");
    document.set(ref("server"), ref("host"), ref("remote"));
    EXPECT_EQ(IniWriter::write(document).to_std_string(),
              "[server]\n"
              "host = 'remote'\n");
}

TEST(IniDocumentTest, EscapedTrailingQuoteNotTreatedAsQuoted) {
    // value = "ab\" : 尾部引号被反斜杠转义，不是闭合引号。
    // detect_quotes 应返回 false，set 重建时不补引号。
    auto document = read_ok("[s]\nx = \"ab\\\"\n");
    EXPECT_EQ(get_ok(document, "s", "x").to_std_string(), "\"ab\\\"");
    document.set(ref("s"), ref("x"), ref("cd"));
    EXPECT_EQ(IniWriter::write(document).to_std_string(),
              "[s]\n"
              "x = cd\n");
}

// 引号值剥引号后的前导空白不再被 strtoll/strtod 静默跳过（与尾随空白对称拒绝）。
TEST(IniDocumentTest, NumericGetRejectsLeadingWhitespace) {
    auto doc = read_ok("[s]\ni = \" 42\"\nd = \" 1.5\"\nok = 42\n");
    EXPECT_FALSE(doc.get_int(ref("s"), ref("i")).is_ok());
    EXPECT_FALSE(doc.get_double(ref("s"), ref("d")).is_ok());
    EXPECT_TRUE(doc.get_int(ref("s"), ref("ok")).is_ok());
}

TEST(IniDocumentTest, ExposesLineKindsAndSectionsInSourceOrder) {
    const auto document = read_ok(
        "# heading\n"
        "root = yes\n"
        "\n"
        "[z]\n"
        "key = \"value\"\n"
        "[a]\n");

    const auto sections = document.sections();
    ASSERT_EQ(sections.size(), 2u);
    EXPECT_EQ(sections[0], "z");
    EXPECT_EQ(sections[1], "a");

    const auto& lines = document.lines();
    ASSERT_EQ(lines.size(), 6u);
    EXPECT_EQ(lines[0].kind, IniLineKind::Comment);
    EXPECT_EQ(lines[1].kind, IniLineKind::KeyValue);
    EXPECT_TRUE(lines[1].section.is_empty());
    EXPECT_EQ(lines[1].key, "root");
    EXPECT_EQ(lines[2].kind, IniLineKind::Blank);
    EXPECT_EQ(lines[3].kind, IniLineKind::Section);
    EXPECT_EQ(lines[3].section, "z");
    EXPECT_EQ(lines[4].kind, IniLineKind::KeyValue);
    EXPECT_EQ(lines[4].section, "z");
    EXPECT_EQ(lines[4].key, "key");
    EXPECT_EQ(lines[4].value, "\"value\"");
    EXPECT_EQ(lines[5].kind, IniLineKind::Section);
    EXPECT_EQ(lines[5].section, "a");
}

TEST(IniDocumentTest, ClearRemovesIndicesAndAllowsReuse) {
    auto document = read_ok("[old]\r\nkey = value\r\n");

    document.clear();

    EXPECT_TRUE(document.lines().empty());
    EXPECT_TRUE(document.sections().empty());
    EXPECT_FALSE(document.has_section(ref("old")));
    EXPECT_TRUE(document.get(ref("old"), ref("key")).is_err());

    document.set(ref("new"), ref("key"), ref("value"));
    EXPECT_EQ(get_ok(document, "new", "key").to_std_string(), "value");
    EXPECT_EQ(IniWriter::write(document).to_std_string(), "[new]\nkey = value\n");
}

TEST(IniDocumentTypedTest, GetIntChecksSigned64BitBounds) {
    const auto document = read_ok(
        "[s]\n"
        "min = -9223372036854775808\n"
        "max = 9223372036854775807\n"
        "too_small = -9223372036854775809\n"
        "too_large = 9223372036854775808\n");

    auto min = document.get_int(ref("s"), ref("min"));
    auto max = document.get_int(ref("s"), ref("max"));
    ASSERT_TRUE(min.is_ok());
    ASSERT_TRUE(max.is_ok());
    EXPECT_EQ(min.unwrap(), std::numeric_limits<ca::i64>::min());
    EXPECT_EQ(max.unwrap(), std::numeric_limits<ca::i64>::max());
    EXPECT_TRUE(document.get_int(ref("s"), ref("too_small")).is_err());
    EXPECT_TRUE(document.get_int(ref("s"), ref("too_large")).is_err());
}
