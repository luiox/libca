#include "libca/csv/csv_reader.hpp"

#include <utility>

#include <gtest/gtest.h>

using namespace ca::csv;
using ca::str::Utf8StringRef;

namespace {

Utf8StringRef ref(const char* text) {
    return Utf8StringRef::from_cstr(text);
}

}  // namespace

TEST(CsvReaderTest, ReadsHeaderAndRows) {
    CsvReaderOptions options;
    options.first_row_is_header = true;

    auto result = CsvReader::read(ref("id,name\n1,Alice\n2,Bob"), options);

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

    auto result = CsvReader::read(ref(
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

TEST(CsvReaderTest, TrimUnquotedSpaceOptionControlsWhitespaceTrimming) {
    // trim_unquoted_space = false（默认）：未引号字段的首尾空白原样保留。
    auto keep = CsvReader::read(ref("a, b ,c"));
    ASSERT_TRUE(keep.is_ok());
    auto keep_doc = std::move(keep).unwrap();
    ASSERT_EQ(keep_doc.rows().size(), 1u);
    EXPECT_EQ(keep_doc.rows()[0][0], "a");
    EXPECT_EQ(keep_doc.rows()[0][1], " b ");
    EXPECT_EQ(keep_doc.rows()[0][2], "c");

    // trim_unquoted_space = true：未引号字段修剪两侧 ASCII 空白。
    CsvReaderOptions options;
    options.trim_unquoted_space = true;
    auto trimmed = CsvReader::read(ref("a, b ,c"), options);
    ASSERT_TRUE(trimmed.is_ok());
    auto trimmed_doc = std::move(trimmed).unwrap();
    ASSERT_EQ(trimmed_doc.rows().size(), 1u);
    EXPECT_EQ(trimmed_doc.rows()[0][0], "a");
    EXPECT_EQ(trimmed_doc.rows()[0][1], "b");
    EXPECT_EQ(trimmed_doc.rows()[0][2], "c");

    // 引号字段不受该选项影响：引号内空白是数据。
    auto quoted = CsvReader::read(ref("\" padded \",\" k \""), options);
    ASSERT_TRUE(quoted.is_ok());
    auto quoted_doc = std::move(quoted).unwrap();
    EXPECT_EQ(quoted_doc.rows()[0][0], " padded ");
    EXPECT_EQ(quoted_doc.rows()[0][1], " k ");
}

TEST(CsvReaderTest, KeepsTrailingEmptyQuotedField) {
    auto single = CsvReader::read(ref("\"\""));
    ASSERT_TRUE(single.is_ok());
    auto single_document = std::move(single).unwrap();
    ASSERT_EQ(single_document.rows().size(), 1u);
    ASSERT_EQ(single_document.rows()[0].size(), 1u);
    EXPECT_EQ(single_document.rows()[0][0], "");

    auto row = CsvReader::read(ref("name,value\nempty,\"\""));
    ASSERT_TRUE(row.is_ok());
    auto row_document = std::move(row).unwrap();
    ASSERT_EQ(row_document.rows().size(), 2u);
    ASSERT_EQ(row_document.rows()[1].size(), 2u);
    EXPECT_EQ(row_document.rows()[1][1], "");
}

TEST(CsvReaderTest, HandlesUtf8Fields) {
    // 中文 UTF-8 字段（张三,30）
    auto result = CsvReader::read(ref("\xE5\xBC\xA0\xE4\xB8\x89,30"));
    ASSERT_TRUE(result.is_ok());
    auto document = std::move(result).unwrap();
    ASSERT_EQ(document.rows().size(), 1u);
    EXPECT_EQ(document.rows()[0][0], "\xE5\xBC\xA0\xE4\xB8\x89");  // 张三
    EXPECT_EQ(document.rows()[0][1], "30");
}

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

TEST(CsvReaderTest, SkipsUtf8Bom) {
    CsvReaderOptions options;
    options.first_row_is_header = true;
    const ca::u8 bom[] = "\xEF\xBB\xBFname,age\nAlice,30";
    auto result = CsvReader::read(ca::str::Utf8StringRef::from_data(
        bom, sizeof(bom) - 1), options);
    ASSERT_TRUE(result.is_ok());
    const auto& doc = std::move(result).unwrap();
    ASSERT_EQ(doc.rows().size(), 1u);
    // header 首列是 "name" 而非 "\uFEFFname"
    std::string first_header(reinterpret_cast<const char*>(doc.header().at(0).data()),
                             doc.header().at(0).byte_length());
    EXPECT_EQ(first_header, "name");
}

TEST(CsvReaderTest, EmptyInputDoesNotCreateHeader) {
    CsvReaderOptions options;
    options.first_row_is_header = true;

    auto result = CsvReader::read(ref(""), options);
    ASSERT_TRUE(result.is_ok());
    const auto document = std::move(result).unwrap();
    EXPECT_FALSE(document.has_header());
    EXPECT_TRUE(document.rows().empty());
}

TEST(CsvReaderTest, PreservesRaggedRowsAndUnquotedEmptyFields) {
    CsvReaderOptions options;
    options.first_row_is_header = true;

    auto result = CsvReader::read(ref("a,b,c\r\nx,\r\ny\r\n,z,"), options);
    ASSERT_TRUE(result.is_ok());
    const auto document = std::move(result).unwrap();
    ASSERT_EQ(document.header().size(), 3u);
    const auto& rows = document.rows();
    ASSERT_EQ(rows.size(), 3u);
    ASSERT_EQ(rows[0].size(), 2u);
    EXPECT_EQ(rows[0][1], "");
    ASSERT_EQ(rows[1].size(), 1u);
    EXPECT_EQ(rows[1][0], "y");
    ASSERT_EQ(rows[2].size(), 3u);
    EXPECT_EQ(rows[2][0], "");
    EXPECT_EQ(rows[2][1], "z");
    EXPECT_EQ(rows[2][2], "");
}
