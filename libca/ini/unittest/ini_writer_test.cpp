#include "libca/ini/ini_writer.hpp"
#include "ini_test_support.hpp"

#include <string>

#include <gtest/gtest.h>

using namespace ca::ini;
using ca::ini::test::read_ok;

TEST(IniWriterTest, RoundTripPreservesOriginalCommentsAndBlankLines) {
    const std::string text =
        "# global comment\r\n"
        "\r\n"
        "[server]\r\n"
        "; host comment\r\n"
        "host = 127.0.0.1 ; keep inline\r\n";

    auto document = read_ok(text.c_str());
    EXPECT_EQ(IniWriter::write(document).to_std_string(), text);
}

TEST(IniWriterTest, LineEndingOverrideToLF) {
    // 原文用 CRLF，强制输出 LF
    auto document = read_ok("[s]\r\nx = 1\r\n");
    IniWriterOptions opts;
    opts.line_ending = "\n";
    EXPECT_EQ(IniWriter::write(document, opts).to_std_string(), "[s]\nx = 1\n");
}

TEST(IniWriterTest, LineEndingDefaultPreservesOriginal) {
    auto document = read_ok("[s]\r\nx = 1\r\n");
    // 默认空 line_ending：保留原 CRLF
    EXPECT_EQ(IniWriter::write(document).to_std_string(), "[s]\r\nx = 1\r\n");
}

TEST(IniWriterTest, PreservesMixedLineEndingsAndMissingFinalNewline) {
    const std::string text = "# heading\r\n[app]\nname = libca";
    const auto document = read_ok(text.c_str());

    EXPECT_EQ(IniWriter::write(document).to_std_string(), text);
}
