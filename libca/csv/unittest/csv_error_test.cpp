#include "libca/csv/csv_reader.hpp"
#include "libca/csv/csv_writer.hpp"

#include <filesystem>
#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace ca::csv;
using ca::str::Utf8StringRef;

namespace {

Utf8StringRef ref(const char* text) {
    return Utf8StringRef::from_cstr(text);
}

}  // namespace

TEST(CsvReaderTest, ReportsUnterminatedQuotedField) {
    auto result = CsvReader::read(ref("id,name\n1,\"Alice"));

    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    // 错误消息应含 "unterminated quoted field"
    std::string msg(reinterpret_cast<const char*>(err.message.data()),
                    reinterpret_cast<const char*>(err.message.data()) + err.message.byte_length());
    EXPECT_NE(msg.find("unterminated quoted field"), std::string::npos);
}

TEST(CsvReaderTest, ParseErrorReportsLocation) {
    // 引号字段跨行未闭合，错误应报告在未闭合处的位置
    auto result = CsvReader::read(ref("a,b\n1,\"unterminated\nstill open"));
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    // 未闭合在末尾，行号应 > 1
    EXPECT_GE(err.location.line, 2u);
}

TEST(CsvReaderTest, ParseErrorOnUnexpectedCharAfterQuote) {
    // 引号闭合后紧跟非分隔符字符
    auto result = CsvReader::read(ref("\"ab\"cd"));
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    EXPECT_EQ(err.location.line, 1u);
    EXPECT_GT(err.location.column, 1u);
}

TEST(CsvWriterTest, ValidateUtf8DefaultReturnsErrOnInvalidBytes) {
    // validate_utf8 默认 true：字段含非法 UTF-8 时 write 返回 Err（不抛异常）。
    const ca::u8 bytes[] = {0xFF, 0xFE, 0xFD};
    CsvDocument document;
    document.add_row(CsvRow({document.intern_field(bytes, 3)}));
    EXPECT_TRUE(CsvWriter::write(document).is_err());
}

TEST(CsvReaderTest, RejectsReservedDelimiterAndQuoteChars) {
    // 验收（issue #234）：CR/LF/NUL 三种字符 × delimiter/quote 全部入口拒绝。
    const char reserved[] = {'\r', '\n', '\0'};
    for (char c : reserved) {
        CsvReaderOptions delim_options;
        delim_options.delimiter = c;
        auto delim_result = CsvReader::read(ref("a b"), delim_options);
        ASSERT_TRUE(delim_result.is_err());
        auto delim_err = std::move(delim_result).unwrap_err();
        std::string delim_msg(
            reinterpret_cast<const char*>(delim_err.message.data()),
            reinterpret_cast<const char*>(delim_err.message.data()) +
                delim_err.message.byte_length());
        EXPECT_NE(delim_msg.find("delimiter"), std::string::npos);

        CsvReaderOptions quote_options;
        quote_options.quote = c;
        auto quote_result = CsvReader::read(ref("a,b"), quote_options);
        ASSERT_TRUE(quote_result.is_err());
        auto quote_err = std::move(quote_result).unwrap_err();
        std::string quote_msg(
            reinterpret_cast<const char*>(quote_err.message.data()),
            reinterpret_cast<const char*>(quote_err.message.data()) +
                quote_err.message.byte_length());
        EXPECT_NE(quote_msg.find("quote"), std::string::npos);
    }

    // 合法自定义分隔符不受影响。
    CsvReaderOptions pipe_options;
    pipe_options.delimiter = '|';
    auto ok_result = CsvReader::read(ref("a|b"), pipe_options);
    ASSERT_TRUE(ok_result.is_ok());
}

TEST(CsvErrorTest, RejectsMatchingDelimiterAndQuote) {
    auto options = CsvReaderOptions::delimited('|');
    options.quote = '|';

    auto result = CsvReader::read(ref("a|b"), options);
    ASSERT_TRUE(result.is_err());
    const auto error = std::move(result).unwrap_err();
    EXPECT_EQ(error.location.line, 1u);
    EXPECT_EQ(error.location.column, 1u);
    EXPECT_NE(error.message.to_std_string().find("must be different"), std::string::npos);
}

TEST(CsvErrorTest, ReportsByteColumnAfterCrLf) {
    auto result = CsvReader::read(ref("a,b\r\n\"\xE6\xB1\x89\"!"));
    ASSERT_TRUE(result.is_err());
    const auto error = std::move(result).unwrap_err();
    EXPECT_EQ(error.location.line, 2u);
    EXPECT_EQ(error.location.column, 6u);
    EXPECT_NE(error.message.to_std_string().find("after closing quote"), std::string::npos);
}

TEST(CsvErrorTest, ReportsFileOpenFailures) {
    // xmake.lua 是普通文件，作为父目录时读写子路径都会失败。
    ASSERT_TRUE(std::filesystem::is_regular_file("xmake.lua"));
    const auto path = ref("xmake.lua/libca_csv_test.csv");

    auto read = CsvReader::read_file(path);
    ASSERT_TRUE(read.is_err());
    EXPECT_NE(std::move(read).unwrap_err().message.to_std_string().find("failed to read CSV file"),
              std::string::npos);

    auto write = CsvWriter::write_file(path, CsvDocument());
    ASSERT_TRUE(write.is_err());
    EXPECT_NE(std::move(write).unwrap_err().to_std_string().find("failed to open CSV file"),
              std::string::npos);
}
