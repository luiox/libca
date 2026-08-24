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

    auto text = CsvWriter::write(document, options).unwrap();

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
    auto text = CsvWriter::write(document).unwrap();
    auto back = CsvReader::read(Utf8StringRef::from_string_view(
        std::string_view(reinterpret_cast<const char*>(text.data()), text.byte_length())));
    ASSERT_TRUE(back.is_ok());
    auto back_doc = std::move(back).unwrap();
    ASSERT_EQ(back_doc.rows().size(), 1u);
    EXPECT_EQ(back_doc.rows()[0][0], "\xE5\xBC\xA0\xE4\xB8\x89");
}

// ============================================================================
// 新增：非 UTF-8 字段保留与 round-trip（验证 intern_raw + validate_utf8=false）
// ============================================================================

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

TEST(CsvIoTest, NonUtf8RoundTripWithValidateOff) {
    // 非 UTF-8 字节经 intern_raw 入池，writer 在 validate_utf8=false 时不校验，
    // 原样输出，可与 reader 完整 round-trip。
    const ca::u8 bytes[] = {0xFF, 0xFE, 0xFD};
    CsvDocument document;
    document.add_row(CsvRow({
        document.intern_field(bytes, 3),
        document.intern_field(reinterpret_cast<const ca::u8*>("ok"), 2),
    }));

    CsvWriterOptions opts;
    opts.validate_utf8 = false;
    auto text = CsvWriter::write(document, opts).unwrap();
    auto back = CsvReader::read(Utf8StringRef::from_string_view(
        std::string_view(reinterpret_cast<const char*>(text.data()), text.byte_length())));
    ASSERT_TRUE(back.is_ok());
    auto back_doc = std::move(back).unwrap();
    ASSERT_EQ(back_doc.rows().size(), 1u);
    ASSERT_EQ(back_doc.rows()[0][0].byte_length(), 3u);
    const ca::u8* p = back_doc.rows()[0][0].data();
    EXPECT_EQ(p[0], 0xFF);
    EXPECT_EQ(p[1], 0xFE);
    EXPECT_EQ(p[2], 0xFD);
    EXPECT_EQ(back_doc.rows()[0][1], "ok");
}

TEST(CsvWriterTest, ValidateUtf8DefaultReturnsErrOnInvalidBytes) {
    // validate_utf8 默认 true：字段含非法 UTF-8 时 write 返回 Err（不抛异常）。
    const ca::u8 bytes[] = {0xFF, 0xFE, 0xFD};
    CsvDocument document;
    document.add_row(CsvRow({document.intern_field(bytes, 3)}));
    EXPECT_TRUE(CsvWriter::write(document).is_err());
}

// ============================================================================
// 新增：DSV 预设（TSV / 任意分隔符）—— csv 模块本质是可配置分隔符的 DSV
// ============================================================================

TEST(CsvDsvPresetTest, TsvRoundTrip) {
    // 构造含 Tab 的字段，验证 Tab 作为分隔符、字段内 Tab 仍被正确引号包裹。
    CsvDocument document;
    document.set_header({"id", "note"});
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("1"), 1),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("hello\tworld"), 11)}));

    auto text = CsvWriter::write(document, CsvWriterOptions::tsv()).unwrap();
    // 字段内含 Tab，需引号包裹；分隔符也是 Tab。
    EXPECT_EQ(S(text), "id\tnote\n1\t\"hello\tworld\"");

    // 用相同预设读回。预设只管分隔符，header 是独立关注点：document 带 header，
    // 这里显式打开 first_row_is_header 让 reader 把首行当标题行。
    auto reader_opt = CsvReaderOptions::tsv();
    reader_opt.first_row_is_header = true;
    auto back = CsvReader::read(Utf8StringRef::from_string_view(
        std::string_view(reinterpret_cast<const char*>(text.data()), text.byte_length())),
        reader_opt);
    ASSERT_TRUE(back.is_ok());
    auto back_doc = std::move(back).unwrap();
    ASSERT_EQ(back_doc.rows().size(), 1u);
    EXPECT_EQ(back_doc.header()[0], "id");
    EXPECT_EQ(back_doc.header()[1], "note");
    EXPECT_EQ(back_doc.rows()[0][0], "1");
    EXPECT_EQ(back_doc.rows()[0][1], "hello\tworld");
}

TEST(CsvDsvPresetTest, TsvPresetSetsTabDelimiter) {
    // 预设只改 delimiter，其余保持默认（quote='"'、trim_unquoted_space=false 等）。
    CsvReaderOptions reader_opt = CsvReaderOptions::tsv();
    EXPECT_EQ(reader_opt.delimiter, '\t');
    EXPECT_EQ(reader_opt.quote, '"');
    EXPECT_FALSE(reader_opt.trim_unquoted_space);

    CsvWriterOptions writer_opt = CsvWriterOptions::tsv();
    EXPECT_EQ(writer_opt.delimiter, '\t');
    EXPECT_EQ(writer_opt.quote, '"');
    EXPECT_EQ(writer_opt.line_ending, "\n");
}

TEST(CsvDsvPresetTest, DelimitedRoundTripWithPipe) {
    // 任意分隔符（管道符）round-trip。
    auto reader_opt = CsvReaderOptions::delimited('|');
    auto writer_opt = CsvWriterOptions::delimited('|');
    ASSERT_EQ(reader_opt.delimiter, '|');
    ASSERT_EQ(writer_opt.delimiter, '|');

    auto result = CsvReader::read(R("a|b|c\n1|2|3"), reader_opt);
    ASSERT_TRUE(result.is_ok());
    auto doc = std::move(result).unwrap();
    ASSERT_EQ(doc.rows().size(), 2u);
    EXPECT_EQ(doc.rows()[0][2], "c");
    EXPECT_EQ(doc.rows()[1][1], "2");

    auto text = CsvWriter::write(doc, writer_opt).unwrap();
    EXPECT_EQ(S(text), "a|b|c\n1|2|3");
}

TEST(CsvDsvPresetTest, CsvPresetMatchesDefault) {
    // csv() 预设应与默认构造完全一致。
    CsvReaderOptions default_reader;
    CsvReaderOptions preset_reader = CsvReaderOptions::csv();
    EXPECT_EQ(default_reader.delimiter, preset_reader.delimiter);
    EXPECT_EQ(default_reader.quote, preset_reader.quote);

    CsvWriterOptions default_writer;
    CsvWriterOptions preset_writer = CsvWriterOptions::csv();
    EXPECT_EQ(default_writer.delimiter, preset_writer.delimiter);
    EXPECT_EQ(default_writer.quote, preset_writer.quote);
    EXPECT_EQ(default_writer.line_ending, preset_writer.line_ending);
}

// UTF-8 BOM（Windows 记事本默认带）不再污染首个 header 字段
//（此前首字段变 "\uFEFFname"，按名取列全部失配且无报错）。
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
