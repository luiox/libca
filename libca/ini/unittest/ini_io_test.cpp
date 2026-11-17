#include "libca/fs/path.hpp"
#include "libca/ini/ini_reader.hpp"
#include "libca/ini/ini_writer.hpp"
#include "ini_test_support.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace ca::ini;
using namespace ca::ini::test;

TEST(IniIoTest, RoundTripsThroughFile) {
    const std::string path = "build/libca_ini_roundtrip_test.ini";
    auto document = read_ok(
        "; file comment\n"
        "[app]\n"
        "name = libca\n");
    document.set(ref("app"), ref("name"), ref("libca.ini"));

    ASSERT_TRUE(IniWriter::write_file(ref(path.c_str()), document).is_ok());
    auto loaded = IniReader::read_file(ref(path.c_str()));
    ASSERT_TRUE(loaded.is_ok());
    auto loaded_doc = std::move(loaded).unwrap();
    EXPECT_EQ(get_ok(loaded_doc, "app", "name").to_std_string(), "libca.ini");

    std::remove(path.c_str());
}

// 非 ASCII（UTF-8）路径经 ca::fs::Path 构造原生路径的真实文件往返回归，理由同 json 用例。
TEST(IniIoTest, RoundTripsThroughUtf8PathFile) {
    const std::string path = "build/libca_ini_utf8路径回归.ini";
    auto document = read_ok("[app]\nname = libca\n");
    document.set(ref("app"), ref("name"), ref("libca.ini"));

    ASSERT_TRUE(IniWriter::write_file(ref(path.c_str()), document).is_ok());
    auto loaded = IniReader::read_file(ref(path.c_str()));
    ASSERT_TRUE(loaded.is_ok());
    auto loaded_doc = std::move(loaded).unwrap();
    EXPECT_EQ(get_ok(loaded_doc, "app", "name").to_std_string(), "libca.ini");

    std::error_code ec;
    std::filesystem::remove(ca::fs::Path::from_utf8_lossy(path).native(), ec);
}
