#include "libca/csv/csv.hpp"

#include <cstdio>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

using namespace ca::csv;

TEST(CsvReaderTest, ReadsHeaderAndRows) {
    CsvReaderOptions options;
    options.first_row_is_header = true;

    auto result = CsvReader::read("id,name\n1,Alice\n2,Bob", options);

    ASSERT_TRUE(result.is_ok());
    auto document = result.unwrap();
    ASSERT_TRUE(document.has_header());
    ASSERT_EQ(document.header().size(), 2u);
    EXPECT_EQ(document.header()[0], "id");
    EXPECT_EQ(document.rows().size(), 2u);
    EXPECT_EQ(document.rows()[1][1], "Bob");
}

TEST(CsvReaderTest, HandlesQuotedCommaQuoteAndMultilineField) {
    CsvReaderOptions options;
    options.first_row_is_header = true;

    auto result = CsvReader::read(
        "id,note\r\n"
        "1,\"hello, \"\"csv\"\"\"\r\n"
        "2,\"line1\nline2\"",
        options);

    ASSERT_TRUE(result.is_ok());
    auto document = result.unwrap();
    ASSERT_EQ(document.rows().size(), 2u);
    EXPECT_EQ(document.rows()[0][1], "hello, \"csv\"");
    EXPECT_EQ(document.rows()[1][1], "line1\nline2");
}

TEST(CsvReaderTest, ReportsUnterminatedQuotedField) {
    auto result = CsvReader::read("id,name\n1,\"Alice");

    ASSERT_TRUE(result.is_err());
    EXPECT_NE(result.unwrap_err().find("unterminated quoted field"), std::string::npos);
}

TEST(CsvWriterTest, WritesEscapedCsvText) {
    CsvDocument document;
    document.set_header({"id", "note"});
    document.add_row(CsvRow({"1", "hello, \"csv\""}));
    document.add_row(CsvRow({"2", "line1\nline2"}));

    CsvWriterOptions options;
    options.line_ending = "\r\n";

    auto text = CsvWriter::write(document, options);

    EXPECT_EQ(text,
              "id,note\r\n"
              "1,\"hello, \"\"csv\"\"\"\r\n"
              "2,\"line1\nline2\"");
}

TEST(CsvIoTest, RoundTripsThroughFile) {
    const std::string path = "build/libca_csv_roundtrip_test.csv";
    CsvDocument document;
    document.set_header({"key", "value"});
    document.add_row(CsvRow({"answer", "42"}));

    ASSERT_TRUE(CsvWriter::write_file(path, document).is_ok());

    CsvReaderOptions options;
    options.first_row_is_header = true;
    auto loaded = CsvReader::read_file(path, options);

    ASSERT_TRUE(loaded.is_ok());
    auto loaded_document = loaded.unwrap();
    ASSERT_EQ(loaded_document.rows().size(), 1u);
    EXPECT_EQ(loaded_document.header()[0], "key");
    EXPECT_EQ(loaded_document.rows()[0][1], "42");

    std::remove(path.c_str());
}
