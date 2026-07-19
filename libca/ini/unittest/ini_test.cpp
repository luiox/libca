#include "libca/ini/ini.hpp"

#include <cstdio>
#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace ca::ini;
using ca::str::Utf8String;
using ca::str::Utf8StringRef;

namespace {

// C 字符串 → Utf8StringRef（测试输入便利）。
Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

// Utf8StringRef → std::string（用于 EXPECT_EQ 与字面量比较）。
// Utf8String 可隐式转 Utf8StringRef，所以本 helper 同时兼容两者。
std::string S(const ca::str::Utf8StringRef& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

// 解析文本，断言成功并取出 document。
IniDocument read_ok(const char* text, IniReaderOptions opts = {}) {
    auto result = IniReader::read(R(text), opts);
    EXPECT_TRUE(result.is_ok()) << "expected parse success";
    return std::move(result).unwrap();
}

// get 并断言成功，返回 value（Utf8StringRef，生命周期绑定 doc；调用方需保持 doc 存活）。
Utf8StringRef get_ok(const IniDocument& doc, const char* section, const char* key) {
    auto r = doc.get(R(section), R(key));
    EXPECT_TRUE(r.is_ok());
    return std::move(r).unwrap();
}

}  // namespace

// ============================================================================
// 保留的旧行为用例（API 适配到 Utf8String）
// ============================================================================

TEST(IniReaderTest, ReadsSectionsAndKeys) {
    auto document = read_ok(
        "root = yes\n"
        "[server]\n"
        "host = 127.0.0.1\n"
        "port: 8080\n");
    ASSERT_TRUE(document.has(R(""), R("root")));
    ASSERT_TRUE(document.has_section(R("server")));
    EXPECT_EQ(S(get_ok(document, "server", "host")), "127.0.0.1");
    EXPECT_EQ(S(get_ok(document, "server", "port")), "8080");
}

TEST(IniWriterTest, RoundTripPreservesOriginalCommentsAndBlankLines) {
    const std::string text =
        "# global comment\r\n"
        "\r\n"
        "[server]\r\n"
        "; host comment\r\n"
        "host = 127.0.0.1 ; keep inline\r\n";

    auto document = read_ok(text.c_str());
    EXPECT_EQ(S(IniWriter::write(document)), text);
}

TEST(IniDocumentTest, SetPreservesInlineCommentAndOrder) {
    auto document = read_ok(
        "# comment\n"
        "[server]\n"
        "host = 127.0.0.1 ; keep inline\n"
        "port = 8080\n");

    document.set(R("server"), R("host"), R("0.0.0.0"));
    document.set(R("server"), R("mode"), R("dev"));

    EXPECT_EQ(S(IniWriter::write(document)),
              "# comment\n"
              "[server]\n"
              "host = 0.0.0.0 ; keep inline\n"
              "port = 8080\n"
              "mode = dev\n");
}

TEST(IniDocumentTest, UpdatingNewKeyPreservesInsertedSpacing) {
    auto document = read_ok("[server]\n");
    document.set(R("server"), R("mode"), R("dev"));
    document.set(R("server"), R("mode"), R("prod"));

    EXPECT_EQ(S(IniWriter::write(document)),
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

    EXPECT_TRUE(document.remove(R("a"), R("x")));
    EXPECT_TRUE(document.remove_section(R("b")));

    EXPECT_EQ(S(IniWriter::write(document)), "[a]\n");
}

TEST(IniDocumentTest, RemoveSectionKeepsNextSectionLeadingComments) {
    auto document = read_ok(
        "[a]\n"
        "x = 1\n"
        "\n"
        "; b docs\n"
        "[b]\n"
        "y = 2\n");

    EXPECT_TRUE(document.remove_section(R("a")));

    EXPECT_EQ(S(IniWriter::write(document)),
              "\n"
              "; b docs\n"
              "[b]\n"
              "y = 2\n");
}

TEST(IniReaderTest, EscapedQuotesDoNotStartInlineComment) {
    auto document = read_ok(
        "[quote]\n"
        "value = \"a \\\" # not comment\" # comment\n"
        "single = 'a \\' ; not comment' ; comment\n");

    EXPECT_EQ(S(get_ok(document, "quote", "value")), "\"a \\\" # not comment\"");
    EXPECT_EQ(S(get_ok(document, "quote", "single")), "'a \\' ; not comment'");
}

TEST(IniDocumentTest, KeysReturnsUniqueNames) {
    auto document = read_ok(
        "[server]\n"
        "host = first\n"
        "port = 1\n"
        "host = second\n");

    auto keys = document.keys(R("server"));

    ASSERT_EQ(keys.size(), 2u);
    EXPECT_EQ(S(keys[0]), "host");
    EXPECT_EQ(S(keys[1]), "port");
    EXPECT_EQ(S(get_ok(document, "server", "host")), "second");
}

TEST(IniIoTest, RoundTripsThroughFile) {
    const std::string path = "build/libca_ini_roundtrip_test.ini";
    auto document = read_ok(
        "; file comment\n"
        "[app]\n"
        "name = libca\n");
    document.set(R("app"), R("name"), R("libca.ini"));

    ASSERT_TRUE(IniWriter::write_file(R(path.c_str()), document).is_ok());
    auto loaded = IniReader::read_file(R(path.c_str()));
    ASSERT_TRUE(loaded.is_ok());
    auto loaded_doc = std::move(loaded).unwrap();
    EXPECT_EQ(S(get_ok(loaded_doc, "app", "name")), "libca.ini");

    std::remove(path.c_str());
}

// ============================================================================
// 新增：类型化访问
// ============================================================================

TEST(IniDocumentTypedTest, GetInt) {
    auto doc = read_ok("[s]\nx = 42\ny = -7\n");
    EXPECT_EQ(doc.get_int(R("s"), R("x")).unwrap_or(0), 42);
    EXPECT_EQ(doc.get_int(R("s"), R("y")).unwrap_or(0), -7);
}

TEST(IniDocumentTypedTest, GetIntStripsQuotes) {
    auto doc = read_ok("[s]\nx = \"8080\"\n");
    // 原始 get 返回带引号的值
    EXPECT_EQ(S(get_ok(doc, "s", "x")), "\"8080\"");
    // get_int 自动剥引号
    EXPECT_EQ(doc.get_int(R("s"), R("x")).unwrap_or(-1), 8080);
}

TEST(IniDocumentTypedTest, GetIntFailsOnNonNumber) {
    auto doc = read_ok("[s]\nx = abc\n");
    EXPECT_TRUE(doc.get_int(R("s"), R("x")).is_err());
}

TEST(IniDocumentTypedTest, GetDouble) {
    auto doc = read_ok("[s]\nx = 3.14\n");
    EXPECT_DOUBLE_EQ(doc.get_double(R("s"), R("x")).unwrap_or(0.0), 3.14);
}

TEST(IniDocumentTypedTest, GetBoolAcceptsCommonForms) {
    auto doc = read_ok("[s]\nt = true\nf = false\nyes = yes\nno = no\non = on\noff = off\n"
                       "one = 1\nzero = 0\nUPPER = TRUE\n");
    EXPECT_TRUE(doc.get_bool(R("s"), R("t")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(R("s"), R("f")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(R("s"), R("yes")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(R("s"), R("no")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(R("s"), R("on")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(R("s"), R("off")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(R("s"), R("one")).unwrap_or(false));
    EXPECT_FALSE(doc.get_bool(R("s"), R("zero")).unwrap_or(true));
    EXPECT_TRUE(doc.get_bool(R("s"), R("UPPER")).unwrap_or(false));
}

TEST(IniDocumentTypedTest, GetBoolFailsOnUnknown) {
    auto doc = read_ok("[s]\nx = maybe\n");
    EXPECT_TRUE(doc.get_bool(R("s"), R("x")).is_err());
}

TEST(IniDocumentTypedTest, GetOrReturnsDefault) {
    auto doc = read_ok("[s]\nx = 1\n");
    EXPECT_EQ(S(doc.get_or(R("s"), R("x"), R("default"))), "1");
    EXPECT_EQ(S(doc.get_or(R("s"), R("missing"), R("default"))), "default");
}

// ============================================================================
// 新增：带引号 value 的 set 修复（痛点 C 回归）
// ============================================================================

TEST(IniDocumentTest, SetPreservesQuotesAroundValue) {
    // 原始 value 带引号，set 新值后应保留引号风格。
    auto document = read_ok("[server]\nhost = \"127.0.0.1\"\n");
    EXPECT_EQ(S(get_ok(document, "server", "host")), "\"127.0.0.1\"");
    document.set(R("server"), R("host"), R("0.0.0.0"));
    EXPECT_EQ(S(IniWriter::write(document)),
              "[server]\n"
              "host = \"0.0.0.0\"\n");
}

TEST(IniDocumentTest, SetPreservesSingleQuotesAroundValue) {
    auto document = read_ok("[server]\nhost = 'localhost'\n");
    document.set(R("server"), R("host"), R("remote"));
    EXPECT_EQ(S(IniWriter::write(document)),
              "[server]\n"
              "host = 'remote'\n");
}

TEST(IniDocumentTest, EscapedTrailingQuoteNotTreatedAsQuoted) {
    // value = "ab\"  —— 尾部引号被反斜杠转义，不是闭合引号。
    // detect_quotes 应返回 false，set 重建时不补引号。
    auto document = read_ok("[s]\nx = \"ab\\\"\n");
    EXPECT_EQ(S(get_ok(document, "s", "x")), "\"ab\\\"");
    document.set(R("s"), R("x"), R("cd"));
    EXPECT_EQ(S(IniWriter::write(document)),
              "[s]\n"
              "x = cd\n");
}

// ============================================================================
// 新增：重复 section / key 检测
// ============================================================================

TEST(IniReaderTest, DefaultKeepsLastOnDuplicateSection) {
    // 默认 KeepLast：同名 section 合并到索引最后一个。
    auto document = read_ok(
        "[s]\nx = 1\n"
        "[s]\ny = 2\n");
    EXPECT_EQ(S(get_ok(document, "s", "y")), "2");
}

TEST(IniReaderTest, ErrorOnDuplicateSectionWhenConfigured) {
    IniReaderOptions opts;
    opts.on_duplicate_section = DuplicatePolicy::Error;
    auto result = IniReader::read(R("[s]\nx=1\n[s]\ny=2\n"), opts);
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    EXPECT_EQ(err.location.line, 3u);
}

TEST(IniReaderTest, ErrorOnDuplicateKeyWhenConfigured) {
    IniReaderOptions opts;
    opts.on_duplicate_key = DuplicatePolicy::Error;
    auto result = IniReader::read(R("[s]\nx=1\nx=2\n"), opts);
    ASSERT_TRUE(result.is_err());
}

TEST(IniReaderTest, DefaultKeepsLastOnDuplicateKey) {
    auto document = read_ok("[s]\nx = first\nx = second\n");
    EXPECT_EQ(S(get_ok(document, "s", "x")), "second");
}

// ============================================================================
// 新增：错误位置（行号）
// ============================================================================

TEST(IniReaderTest, ParseErrorReportsLineNumber) {
    auto result = IniReader::read(R("ok = 1\n[s]\nx = 1\n[s\n"));
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    // 缺 ']' 的 section 在第 4 行
    EXPECT_EQ(err.location.line, 4u);
}

TEST(IniReaderTest, ParseErrorOnMissingSeparator) {
    EXPECT_TRUE(IniReader::read(R("[s]\nbadline\n")).is_err());
}

// ============================================================================
// 新增：UTF-8 key/value
// ============================================================================

TEST(IniReaderTest, HandlesUtf8KeysAndValues) {
    auto document = read_ok("[\xE6\x9C\x8D\xE5\x8A\xA1]\n"
                            "\xE5\x9C\xB0\xE5\x9D\x80 = \xE5\x8C\x97\xE4\xBA\xAC\n");
    // section "服务"，key "地址"，value "北京"
    ASSERT_TRUE(document.has_section(R("\xE6\x9C\x8D\xE5\x8A\xA1")));
    EXPECT_EQ(S(get_ok(document, "\xE6\x9C\x8D\xE5\x8A\xA1", "\xE5\x9C\xB0\xE5\x9D\x80")),
              "\xE5\x8C\x97\xE4\xBA\xAC");
}

// ============================================================================
// 新增：IniWriterOptions.line_ending 统一覆盖
// ============================================================================

TEST(IniWriterTest, LineEndingOverrideToLF) {
    // 原文用 CRLF，强制输出 LF
    auto document = read_ok("[s]\r\nx = 1\r\n");
    IniWriterOptions opts;
    opts.line_ending = "\n";
    EXPECT_EQ(S(IniWriter::write(document, opts)), "[s]\nx = 1\n");
}

TEST(IniWriterTest, LineEndingDefaultPreservesOriginal) {
    auto document = read_ok("[s]\r\nx = 1\r\n");
    // 默认空 line_ending：保留原 CRLF
    EXPECT_EQ(S(IniWriter::write(document)), "[s]\r\nx = 1\r\n");
}
