#include "libca/ini/ini_reader.hpp"
#include "libca/ini/ini_writer.hpp"
#include "ini_test_support.hpp"

#include <filesystem>
#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace ca::ini;
using namespace ca::ini::test;
using ca::str::Utf8String;

TEST(IniReaderTest, ErrorOnDuplicateSectionWhenConfigured) {
    IniReaderOptions opts;
    opts.on_duplicate_section = DuplicatePolicy::Error;
    auto result = IniReader::read(ref("[s]\nx=1\n[s]\ny=2\n"), opts);
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    EXPECT_EQ(err.location.line, 3u);
}

TEST(IniReaderTest, ErrorOnDuplicateKeyWhenConfigured) {
    IniReaderOptions opts;
    opts.on_duplicate_key = DuplicatePolicy::Error;
    auto result = IniReader::read(ref("[s]\nx=1\nx=2\n"), opts);
    ASSERT_TRUE(result.is_err());
}

TEST(IniReaderTest, ParseErrorReportsLineNumber) {
    auto result = IniReader::read(ref("ok = 1\n[s]\nx = 1\n[s\n"));
    ASSERT_TRUE(result.is_err());
    auto err = std::move(result).unwrap_err();
    // 缺 ']' 的 section 在第 4 行
    EXPECT_EQ(err.location.line, 4u);
}

TEST(IniReaderTest, ParseErrorOnMissingSeparator) {
    EXPECT_TRUE(IniReader::read(ref("[s]\nbadline\n")).is_err());
}

// 非法 UTF-8（GBK 常见字节）此前经 intern_std 的校验构造抛 std::runtime_error，
// 逃出返回 Result 的 read()，调用方直接 terminate。
TEST(IniReaderTest, InvalidUtf8ReportsParseErrorInsteadOfThrowing) {
    const char gb[] = "[s]\nname=\xD5\xC5\xC8\xFD\n";  // GBK "张三"
    auto raw = Utf8String::from_data_unchecked(
        reinterpret_cast<const ca::u8*>(gb), sizeof(gb) - 1);
    auto result = IniReader::read(raw.ref());
    ASSERT_TRUE(result.is_err());
    EXPECT_NE(std::move(result).unwrap_err().message.to_std_string().find("invalid UTF-8"), std::string::npos);
}

// 行内注释里的 '='/':' 不再被误当成分隔符吞进 key。
TEST(IniReaderTest, InlineCommentDoesNotHijackSeparator) {
    // 正常形态：注释跟在完整 key=value 之后。
    {
        auto doc = read_ok("port = 8080  # default: 8080\n");
        EXPECT_EQ(get_ok(doc, "", "port").to_std_string(), "8080");
    }
    // 注释先于任何分隔符出现：分隔符查找限定在注释之前，无分隔符报错
    //（而非把 "port  # default" 当 key）。
    {
        auto result = IniReader::read(ref("port  # default: 8080\n"));
        ASSERT_TRUE(result.is_err());
        EXPECT_NE(std::move(result).unwrap_err().message.to_std_string().find("misses '=' or ':'"),
                  std::string::npos);
    }
    // 分号注释同理。
    {
        auto result = IniReader::read(ref("host ; see http://x:8080\n"));
        ASSERT_TRUE(result.is_err());
    }
    // 无空白前缀的 '#' 不是注释起始（strict_whitespace 默认 true），key 保留。
    {
        auto doc = read_ok("my#key = v\n");
        EXPECT_EQ(get_ok(doc, "", "my#key").to_std_string(), "v");
    }
}

TEST(IniErrorTest, ReportsDisabledGlobalKeyOnItsSourceLine) {
    IniReaderOptions options;
    options.allow_global_keys = false;

    auto result = IniReader::read(ref("# heading\r\nroot = 1\r\n[app]\r\nkey = 2"), options);
    ASSERT_TRUE(result.is_err());
    const auto error = std::move(result).unwrap_err();
    EXPECT_EQ(error.location.line, 2u);
    EXPECT_EQ(error.location.column, 1u);
    EXPECT_NE(error.message.to_std_string().find("global key/value is disabled"), std::string::npos);

    EXPECT_TRUE(IniReader::read(ref("[app]\r\nkey = 2"), options).is_ok());
}

TEST(IniErrorTest, ReportsFileOpenFailures) {
    // xmake.lua 是普通文件，作为父目录时读写子路径都会失败。
    ASSERT_TRUE(std::filesystem::is_regular_file("xmake.lua"));
    const auto path = ref("xmake.lua/libca_ini_test.ini");

    auto read = IniReader::read_file(path);
    ASSERT_TRUE(read.is_err());
    EXPECT_NE(std::move(read).unwrap_err().message.to_std_string().find("failed to read INI file"),
              std::string::npos);

    auto write = IniWriter::write_file(path, IniDocument());
    ASSERT_TRUE(write.is_err());
    EXPECT_NE(std::move(write).unwrap_err().to_std_string().find("failed to open INI file"),
              std::string::npos);
}
