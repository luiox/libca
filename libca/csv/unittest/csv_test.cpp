#include "libca/csv/csv.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace ca::csv;
using ca::str::Utf8StringRef;

namespace {

// C 字符串 → Utf8StringRef（CsvReader::read 的输入）。
Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

// Utf8String → std::string（用于和字面量比较 CsvWriter::write 的输出）。
std::string S(const ca::str::Utf8String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

}  // namespace

TEST(CsvReaderTest, ReadsHeaderAndRows) {
    CsvReaderOptions options;
    options.first_row_is_header = true;

    auto result = CsvReader::read(R("id,name\n1,Alice\n2,Bob"), options);

    ASSERT_TRUE(result.is_ok());
    auto document = std::move(result).unwrap();
    ASSERT_TRUE(document.has_header());
    ASSERT_EQ(document.header().size(), 2u);
    EXPECT_EQ(document.header()[0], "id");
    EXPECT_EQ(document.rows().size(), 2u);
    EXPECT_EQ(document.rows()[1][1], "Bob");
}

TEST(CsvReaderTest, HandlesQuotedCommaQuoteAndMultilineField) {
    CsvReaderOptions options;
    options.first_row_is_header = true;

    auto result = CsvReader::read(R(
        "id,note\r\n"
        "1,\"hello, \"\"csv\"\"\"\r\n"
        "2,\"line1\nline2\""),
        options);

    ASSERT_TRUE(result.is_ok());
    auto document = std::move(result).unwrap();
    ASSERT_EQ(document.rows().size(), 2u);
    EXPECT_EQ(document.rows()[0][1], "hello, \"csv\"");
    EXPECT_EQ(document.rows()[1][1], "line1\nline2");
}

TEST(CsvReaderTest, ReportsUnterminatedQuotedField) {
    auto result = CsvReader::read(R("id,name\n1,\"Alice"));

    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    // 错误消息应含 "unterminated quoted field"
    std::string msg(reinterpret_cast<const char*>(err.message.data()),
                    reinterpret_cast<const char*>(err.message.data()) + err.message.byte_length());
    EXPECT_NE(msg.find("unterminated quoted field"), std::string::npos);
}

TEST(CsvReaderTest, KeepsTrailingEmptyQuotedField) {
    auto single = CsvReader::read(R("\"\""));
    ASSERT_TRUE(single.is_ok());
    auto single_document = std::move(single).unwrap();
    ASSERT_EQ(single_document.rows().size(), 1u);
    ASSERT_EQ(single_document.rows()[0].size(), 1u);
    EXPECT_EQ(single_document.rows()[0][0], "");

    auto row = CsvReader::read(R("name,value\nempty,\"\""));
    ASSERT_TRUE(row.is_ok());
    auto row_document = std::move(row).unwrap();
    ASSERT_EQ(row_document.rows().size(), 2u);
    ASSERT_EQ(row_document.rows()[1].size(), 2u);
    EXPECT_EQ(row_document.rows()[1][1], "");
}

TEST(CsvWriterTest, WritesEscapedCsvText) {
    CsvDocument document;
    document.set_header({"id", "note"});
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("1"), 1),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("hello, \"csv\""), 12)}));
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("2"), 1),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("line1\nline2"), 11)}));

    CsvWriterOptions options;
    options.line_ending = "\r\n";

    auto text = CsvWriter::write(document, options);

    EXPECT_EQ(S(text),
              "id,note\r\n"
              "1,\"hello, \"\"csv\"\"\"\r\n"
              "2,\"line1\nline2\"");
}

TEST(CsvIoTest, RoundTripsThroughFile) {
    const std::string path = "build/libca_csv_roundtrip_test.csv";
    CsvDocument document;
    document.set_header({"key", "value"});
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("answer"), 6),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("42"), 2)}));

    ASSERT_TRUE(CsvWriter::write_file(R(path.c_str()), document).is_ok());

    CsvReaderOptions options;
    options.first_row_is_header = true;
    auto loaded = CsvReader::read_file(R(path.c_str()), options);

    ASSERT_TRUE(loaded.is_ok());
    auto loaded_document = std::move(loaded).unwrap();
    ASSERT_EQ(loaded_document.rows().size(), 1u);
    EXPECT_EQ(loaded_document.header()[0], "key");
    EXPECT_EQ(loaded_document.rows()[0][1], "42");

    std::remove(path.c_str());
}

// ============================================================================
// 新增：错误带行号/列号
// ============================================================================

TEST(CsvReaderTest, ParseErrorReportsLocation) {
    // 引号字段跨行未闭合，错误应报告在未闭合处的位置
    auto result = CsvReader::read(R("a,b\n1,\"unterminated\nstill open"));
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    // 未闭合在末尾，行号应 > 1
    EXPECT_GE(err.location.line, 2u);
}

TEST(CsvReaderTest, ParseErrorOnUnexpectedCharAfterQuote) {
    // 引号闭合后紧跟非分隔符字符
    auto result = CsvReader::read(R("\"ab\"cd"));
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    EXPECT_EQ(err.location.line, 1u);
    EXPECT_GT(err.location.column, 1u);
}

// ============================================================================
// 新增：UTF-8 字段（IO 边界走 Utf8String/Utf8StringRef，字段内部按字节保留）
// ============================================================================

TEST(CsvReaderTest, HandlesUtf8Fields) {
    // 中文 UTF-8 字段（张三,30）
    auto result = CsvReader::read(R("\xE5\xBC\xA0\xE4\xB8\x89,30"));
    ASSERT_TRUE(result.is_ok());
    auto document = std::move(result).unwrap();
    ASSERT_EQ(document.rows().size(), 1u);
    EXPECT_EQ(document.rows()[0][0], "\xE5\xBC\xA0\xE4\xB8\x89");  // 张三
    EXPECT_EQ(document.rows()[0][1], "30");
}

TEST(CsvIoTest, Utf8RoundTrip) {
    CsvDocument document;
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("\xE5\xBC\xA0\xE4\xB8\x89"), 6),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("30"), 2)}));
    auto text = CsvWriter::write(document);
    auto back = CsvReader::read(Utf8StringRef::from_string_view(
        std::string_view(reinterpret_cast<const char*>(text.data()), text.byte_length())));
    ASSERT_TRUE(back.is_ok());
    auto back_doc = std::move(back).unwrap();
    ASSERT_EQ(back_doc.rows().size(), 1u);
    EXPECT_EQ(back_doc.rows()[0][0], "\xE5\xBC\xA0\xE4\xB8\x89");
}

// ============================================================================
// 新增：非 UTF-8 字段保留（验证 intern_raw 保留任意字节）
// ============================================================================
// 注意：CsvWriter 当前输出 Utf8String（构造时校验 UTF-8），无法输出非 UTF-8 字节——
// 这是 csv 模块独立的限制，不在 Arena 迁移范围内。此处仅验证 reader 经 intern_raw
// 入池后能原样保留任意字节，跳过 writer round-trip。

TEST(CsvReaderTest, PreservesNonUtf8Bytes) {
    // 非 UTF-8 字节序列（\xFF\xFE\xFD 不是合法 UTF-8），reader 应原样保留。
    const ca::u8 input[] = {'a', ',', 0xFF, 0xFE, 0xFD, '\n', 'b', ',', 'o', 'k'};
    auto result = CsvReader::read(Utf8StringRef::from_data(input, sizeof(input)));
    ASSERT_TRUE(result.is_ok());
    auto doc = std::move(result).unwrap();
    ASSERT_EQ(doc.rows().size(), 2u);
    ASSERT_EQ(doc.rows()[0][0], "a");
    ASSERT_EQ(doc.rows()[0][1].byte_length(), 3u);
    const ca::u8* p = doc.rows()[0][1].data();
    EXPECT_EQ(p[0], 0xFF);
    EXPECT_EQ(p[1], 0xFE);
    EXPECT_EQ(p[2], 0xFD);
    EXPECT_EQ(doc.rows()[1][0], "b");
    EXPECT_EQ(doc.rows()[1][1], "ok");
}
