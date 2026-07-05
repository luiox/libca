#include "libca/ini/ini.hpp"

#include <cstdio>
#include <string>

#include <gtest/gtest.h>

using namespace ca::ini;

TEST(IniReaderTest, ReadsSectionsAndKeys) {
    auto result = IniReader::read(
        "root = yes\n"
        "[server]\n"
        "host = 127.0.0.1\n"
        "port: 8080\n");

    ASSERT_TRUE(result.is_ok());
    auto document = result.unwrap();
    ASSERT_TRUE(document.has("", "root"));
    ASSERT_TRUE(document.has_section("server"));
    EXPECT_EQ(document.get("server", "host").unwrap(), "127.0.0.1");
    EXPECT_EQ(document.get("server", "port").unwrap(), "8080");
}

TEST(IniWriterTest, RoundTripPreservesOriginalCommentsAndBlankLines) {
    const std::string text =
        "# global comment\r\n"
        "\r\n"
        "[server]\r\n"
        "; host comment\r\n"
        "host = 127.0.0.1 ; keep inline\r\n";

    auto result = IniReader::read(text);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(IniWriter::write(result.unwrap()), text);
}

TEST(IniDocumentTest, SetPreservesInlineCommentAndOrder) {
    auto result = IniReader::read(
        "# comment\n"
        "[server]\n"
        "host = 127.0.0.1 ; keep inline\n"
        "port = 8080\n");
    ASSERT_TRUE(result.is_ok());
    auto document = result.unwrap();

    document.set("server", "host", "0.0.0.0");
    document.set("server", "mode", "dev");

    EXPECT_EQ(IniWriter::write(document),
              "# comment\n"
              "[server]\n"
              "host = 0.0.0.0 ; keep inline\n"
              "port = 8080\n"
              "mode = dev\n");
}

TEST(IniDocumentTest, RemoveKeyAndSection) {
    auto result = IniReader::read(
        "[a]\n"
        "x = 1\n"
        "[b]\n"
        "y = 2\n"
        "; b comment\n");
    ASSERT_TRUE(result.is_ok());
    auto document = result.unwrap();

    EXPECT_TRUE(document.remove("a", "x"));
    EXPECT_TRUE(document.remove_section("b"));

    EXPECT_EQ(IniWriter::write(document), "[a]\n");
}

TEST(IniIoTest, RoundTripsThroughFile) {
    const std::string path = "build/libca_ini_roundtrip_test.ini";
    auto result = IniReader::read(
        "; file comment\n"
        "[app]\n"
        "name = libca\n");
    ASSERT_TRUE(result.is_ok());
    auto document = result.unwrap();
    document.set("app", "name", "libca.ini");

    ASSERT_TRUE(IniWriter::write_file(path, document).is_ok());
    auto loaded = IniReader::read_file(path);

    ASSERT_TRUE(loaded.is_ok());
    EXPECT_EQ(loaded.unwrap().get("app", "name").unwrap(), "libca.ini");

    std::remove(path.c_str());
}
