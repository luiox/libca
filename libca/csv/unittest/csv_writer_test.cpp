#include "libca/csv/csv_writer.hpp"

#include <utility>

#include <gtest/gtest.h>

using namespace ca::csv;

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

    auto text = CsvWriter::write(document, options).unwrap();

    EXPECT_EQ(text.to_std_string(),
              "id,note\r\n"
              "1,\"hello, \"\"csv\"\"\"\r\n"
              "2,\"line1\nline2\"");
}

TEST(CsvWriterTest, OmitsHeaderWithoutLeavingAnEmptyLine) {
    CsvDocument document;
    document.set_header({"name"});
    document.add_row(CsvRow({document.intern_field(reinterpret_cast<const ca::u8*>("a"), 1)}));
    document.add_row(CsvRow({document.intern_field(reinterpret_cast<const ca::u8*>("b"), 1)}));
    CsvWriterOptions options;
    options.write_header = false;

    auto result = CsvWriter::write(document, options);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap().to_std_string(), "a\nb");
}

TEST(CsvWriterTest, AlwaysQuoteUsesConfiguredQuoteForPlainAndEmptyFields) {
    CsvDocument document;
    document.set_header({"name", "note", "empty"});
    document.add_row(CsvRow({
        document.intern_field(reinterpret_cast<const ca::u8*>("a"), 1),
        document.intern_field(reinterpret_cast<const ca::u8*>("it's"), 4),
        document.intern_field(reinterpret_cast<const ca::u8*>(""), 0),
    }));
    auto options = CsvWriterOptions::delimited(';');
    options.quote = '\'';
    options.always_quote = true;

    auto result = CsvWriter::write(document, options);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap().to_std_string(), "'name';'note';'empty'\n'a';'it''s';''");
}
