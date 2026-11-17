#include "libca/fs/path.hpp"
#include "libca/csv/csv_reader.hpp"
#include "libca/csv/csv_writer.hpp"

#include <cstdio>
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

TEST(CsvIoTest, RoundTripsThroughFile) {
    const std::string path = "build/libca_csv_roundtrip_test.csv";
    CsvDocument document;
    document.set_header({"key", "value"});
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("answer"), 6),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("42"), 2)}));

    ASSERT_TRUE(CsvWriter::write_file(ref(path.c_str()), document).is_ok());

    CsvReaderOptions options;
    options.first_row_is_header = true;
    auto loaded = CsvReader::read_file(ref(path.c_str()), options);

    ASSERT_TRUE(loaded.is_ok());
    auto loaded_document = std::move(loaded).unwrap();
    ASSERT_EQ(loaded_document.rows().size(), 1u);
    EXPECT_EQ(loaded_document.header()[0], "key");
    EXPECT_EQ(loaded_document.rows()[0][1], "42");

    std::remove(path.c_str());
}

TEST(CsvIoTest, Utf8RoundTrip) {
    CsvDocument document;
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("\xE5\xBC\xA0\xE4\xB8\x89"), 6),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("30"), 2)}));
    auto text = CsvWriter::write(document).unwrap();
    auto back = CsvReader::read(text.ref());
    ASSERT_TRUE(back.is_ok());
    auto back_doc = std::move(back).unwrap();
    ASSERT_EQ(back_doc.rows().size(), 1u);
    EXPECT_EQ(back_doc.rows()[0][0], "\xE5\xBC\xA0\xE4\xB8\x89");
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
    auto back = CsvReader::read(text.ref());
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
    EXPECT_EQ(text.to_std_string(), "id\tnote\n1\t\"hello\tworld\"");

    // 用相同预设读回。预设只管分隔符，header 是独立关注点：document 带 header，
    // 这里显式打开 first_row_is_header 让 reader 把首行当标题行。
    auto reader_opt = CsvReaderOptions::tsv();
    reader_opt.first_row_is_header = true;
    auto back = CsvReader::read(text.ref(),
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

    auto result = CsvReader::read(ref("a|b|c\n1|2|3"), reader_opt);
    ASSERT_TRUE(result.is_ok());
    auto doc = std::move(result).unwrap();
    ASSERT_EQ(doc.rows().size(), 2u);
    EXPECT_EQ(doc.rows()[0][2], "c");
    EXPECT_EQ(doc.rows()[1][1], "2");

    auto text = CsvWriter::write(doc, writer_opt).unwrap();
    EXPECT_EQ(text.to_std_string(), "a|b|c\n1|2|3");
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

// 非 ASCII（UTF-8）路径经 ca::fs::Path 构造原生路径的真实文件往返回归，理由同 json 用例。
TEST(CsvIoTest, RoundTripsThroughUtf8PathFile) {
    const std::string path = "build/libca_csv_utf8路径回归.csv";
    CsvDocument document;
    document.set_header({"key", "value"});
    document.add_row(CsvRow({document.intern_field(
                            reinterpret_cast<const ca::u8*>("answer"), 6),
                        document.intern_field(
                            reinterpret_cast<const ca::u8*>("42"), 2)}));

    ASSERT_TRUE(CsvWriter::write_file(ref(path.c_str()), document).is_ok());

    CsvReaderOptions options;
    options.first_row_is_header = true;
    auto loaded = CsvReader::read_file(ref(path.c_str()), options);

    ASSERT_TRUE(loaded.is_ok());
    auto loaded_document = std::move(loaded).unwrap();
    ASSERT_EQ(loaded_document.rows().size(), 1u);
    EXPECT_EQ(loaded_document.rows()[0][1], "42");

    std::error_code ec;
    std::filesystem::remove(ca::fs::Path::from_utf8_lossy(path).native(), ec);
}
