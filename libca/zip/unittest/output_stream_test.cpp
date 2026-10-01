#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

#include "libca/zip/entry.hpp"
#include "libca/zip/file.hpp"
#include "libca/zip/output_stream.hpp"
#include "zip_fixtures.hpp"

namespace {

using ca::zip::ZipEntry;
using ca::zip::ZipError;
using ca::zip::ZipFile;
using ca::zip::ZipOutputStream;
using zip_test::expect_ok;
using zip_test::expect_value;
using namespace zip_test;

TEST(ZipOutputStreamTest, WriteReadStored) {
    const auto outPath = temp_path("zos_stored.zip");
    {
        ZipOutputStream zos(outPath.string());
        expect_ok(zos.put_next_entry(ZipEntry("test.txt", 0, 5, 0, 0, 0)), "put: ");
        const std::string content = "Hello";
        expect_ok(zos.write(reinterpret_cast<const ca::u8*>(content.data()), content.size()), "write: ");
        expect_ok(zos.close_entry(), "close: ");
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 1u);
    auto        data = expect_value(zf.read("test.txt"), "read: ");
    std::string result(data.begin(), data.end());
    EXPECT_EQ(result, "Hello");
}

TEST(ZipOutputStreamTest, WriteReadDeflated) {
    const auto outPath = temp_path("zos_deflated.zip");
    {
        ZipOutputStream zos(outPath.string());
        expect_ok(zos.put_next_entry(ZipEntry("data.bin", 0, 1000, 8, 0, 0)), "put: ");
        std::vector<ca::u8> data(1000);
        for (int i = 0; i < 1000; i++)
            data[i] = static_cast<ca::u8>(i % 256);
        expect_ok(zos.write(data), "write: ");
        expect_ok(zos.close_entry(), "close: ");
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 1u);
    auto result = expect_value(zf.read("data.bin"), "read: ");
    EXPECT_EQ(result.size(), 1000u);
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(result[i], static_cast<ca::u8>(i % 256));
    }
}

TEST(ZipOutputStreamTest, RejectsMoreThanUint16Entries)
{
    // 回归：EOCD 条目数字段为 u16，无 ZIP64 写路径时第 65536 条起计数回绕，
    // 会产出目录损坏却"看似成功"的归档；改为显式失败。
    const auto outPath = temp_path("zos_entry_overflow.zip");
    ZipOutputStream zos(outPath.string());
    const std::string content = "x";
    for (int i = 0; i < 65535; ++i) {
        zos.put_next_entry(ZipEntry("e" + std::to_string(i) + ".txt", 0,
                                    static_cast<ca::u32>(content.size()), 0, 0, 0));
        zos.write(reinterpret_cast<const ca::u8*>(content.data()), content.size());
        zos.close_entry();
    }
    zos.put_next_entry(ZipEntry("overflow.txt", 0, 1, 0, 0, 0));
    zos.write(reinterpret_cast<const ca::u8*>(content.data()), 1);
    EXPECT_THROW(zos.close_entry(), std::runtime_error);
}

TEST(ZipOutputStreamTest, MultipleEntries)
{
    const auto outPath = temp_path("zos_multi.zip");
    {
        ZipOutputStream zos(outPath.string());

        expect_ok(zos.put_next_entry(ZipEntry("a.txt", 0, 3, 0, 0, 0)), "put a: ");
        expect_ok(zos.write(std::vector<ca::u8>{'a', 'b', 'c'}), "write a: ");
        expect_ok(zos.close_entry(), "close a: ");

        expect_ok(zos.put_next_entry(ZipEntry("b.txt", 0, 3, 8, 0, 0)), "put b: ");
        expect_ok(zos.write(std::vector<ca::u8>{'d', 'e', 'f'}), "write b: ");
        expect_ok(zos.close_entry(), "close b: ");
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 2u);
    EXPECT_EQ(expect_value(zf.read("a.txt"), "read a: "), std::vector<ca::u8>({'a', 'b', 'c'}));
    EXPECT_EQ(expect_value(zf.read("b.txt"), "read b: "), std::vector<ca::u8>({'d', 'e', 'f'}));
}

TEST(ZipOutputStreamTest, EmptyEntry) {
    const auto outPath = temp_path("zos_empty.zip");
    {
        ZipOutputStream zos(outPath.string());
        expect_ok(zos.put_next_entry(ZipEntry("empty.txt", 0, 0, 0, 0, 0)), "put: ");
        expect_ok(zos.close_entry(), "close: ");
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 1u);
    EXPECT_TRUE(expect_value(zf.read("empty.txt"), "read: ").empty());
}

TEST(ZipOutputStreamTest, DeflatedDataIntegrity) {
    const auto outPath = temp_path("zos_integrity.zip");

    std::string original;
    for (int i = 0; i < 10000; i++) {
        original += "The quick brown fox jumps over the lazy dog. ";
    }

    {
        ZipOutputStream zos(outPath.string());
        expect_ok(zos.put_next_entry(ZipEntry("large.txt", 0, static_cast<ca::u32>(original.size()), 8, 0, 0)),
                  "put: ");
        expect_ok(zos.write(reinterpret_cast<const ca::u8*>(original.data()), original.size()), "write: ");
        expect_ok(zos.close_entry(), "close: ");
    }

    ZipFile     zf(outPath.string());
    auto        data = expect_value(zf.read("large.txt"), "read: ");
    std::string result(data.begin(), data.end());
    EXPECT_EQ(result.size(), original.size());
    EXPECT_EQ(result, original);
}

TEST(ZipOutputStreamTest, CompressionLevels) {
    const std::string data = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

    for (const int level : {0, 1, 6, 9}) {
        const auto outPath = temp_path("zos_level" + std::to_string(level) + ".zip");
        {
            ZipOutputStream zos(outPath.string());
            expect_ok(zos.set_level(level), "set_level: ");
            expect_ok(zos.put_next_entry(ZipEntry("test.txt", 0, static_cast<ca::u32>(data.size()), 8, 0, 0)), "put: ");
            expect_ok(zos.write(reinterpret_cast<const ca::u8*>(data.data()), data.size()), "write: ");
            expect_ok(zos.close_entry(), "close: ");
        }
        ZipFile     zf(outPath.string());
        auto        result = expect_value(zf.read("test.txt"), "read: ");
        std::string content(result.begin(), result.end());
        EXPECT_EQ(content, data) << "Failed for compression level " << level;
    }
}

TEST(ZipOutputStreamTest, ManyEntries) {
    const auto    outPath     = temp_path("zos_many_entries.zip");
    constexpr int kEntryCount = 50;
    {
        ZipOutputStream zos(outPath.string());
        for (int i = 0; i < kEntryCount; i++) {
            const std::string name = "file" + std::to_string(i) + ".txt";
            expect_ok(zos.put_next_entry(ZipEntry(name, 0, 4, 0, 0, 0)), "put: ");
            const std::string content = std::to_string(i);
            expect_ok(zos.write(reinterpret_cast<const ca::u8*>(content.data()), content.size()), "write: ");
            expect_ok(zos.close_entry(), "close: ");
        }
    }
    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), static_cast<size_t>(kEntryCount));
    auto names = zf.entries();
    EXPECT_EQ(names.size(), static_cast<size_t>(kEntryCount));
    for (int i = 0; i < kEntryCount; i++) {
        const std::string expectedName = "file" + std::to_string(i) + ".txt";
        EXPECT_NE(std::find(names.begin(), names.end(), expectedName), names.end());
        auto        data = expect_value(zf.read(expectedName), "read: ");
        std::string actual(data.begin(), data.end());
        EXPECT_EQ(actual, std::to_string(i));
    }
}

TEST(ZipOutputStreamTest, SpecialCharacters) {
    const auto               outPath = temp_path("zos_special_chars.zip");
    std::vector<std::string> names   = {
        "file with spaces.txt",
        "file_with_underscores.txt",
        "a/b/file_in_subdir.txt",
        "deeply/nested/path/file.txt",
        "Dollar$ign.txt",
    };
    {
        ZipOutputStream zos(outPath.string());
        for (const auto& name : names) {
            expect_ok(zos.put_next_entry(ZipEntry(name, 0, static_cast<ca::u32>(name.size()), 0, 0, 0)), "put: ");
            expect_ok(zos.write(reinterpret_cast<const ca::u8*>(name.data()), name.size()), "write: ");
            expect_ok(zos.close_entry(), "close: ");
        }
    }
    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), names.size());
    for (const auto& name : names) {
        auto        data = expect_value(zf.read(name), "read: ");
        std::string actual(data.begin(), data.end());
        EXPECT_EQ(actual, name) << "Failed for entry: " << name;
    }
}

TEST(ZipOutputStreamTest, RoundtripMetadata) {
    const auto          outPath     = temp_path("zos_metadata.zip");
    std::vector<ca::u8> data        = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    const ca::u32       expectedCrc = ::crc32(0, data.data(), static_cast<uInt>(data.size()));
    {
        ZipOutputStream zos(outPath.string());
        expect_ok(zos.put_next_entry(ZipEntry("hello.dat", 0, static_cast<ca::u32>(data.size()), 8, 0, 0)), "put: ");
        expect_ok(zos.write(data), "write: ");
        expect_ok(zos.close_entry(), "close: ");
    }
    auto        fileData = read_file_bytes(outPath);
    ZipFile     zf(fileData);
    const auto* entry = zf.get_entry("hello.dat");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->crc32(), expectedCrc);
    EXPECT_EQ(entry->uncompressed_size(), static_cast<ca::u32>(data.size()));
}

TEST(ZipOutputStreamTest, InvalidLevelReturnsErr) {
    ZipOutputStream zos(temp_path("zos_bad_level.zip").string());

    auto tooHigh = zos.set_level(10);
    ASSERT_TRUE(tooHigh.is_err());
    EXPECT_EQ(tooHigh.unwrap_err().code, ZipError::INVALID_ARGUMENT);

    auto tooLow = zos.set_level(-2);
    ASSERT_TRUE(tooLow.is_err());
    EXPECT_EQ(tooLow.unwrap_err().code, ZipError::INVALID_ARGUMENT);

    EXPECT_TRUE(zos.set_level(6).is_ok());
}

TEST(ZipOutputStreamTest, WriteWithoutEntryReturnsErr) {
    ZipOutputStream zos(temp_path("zos_no_entry.zip").string());
    const ca::u8    byte = 'x';

    auto rejected = zos.write(&byte, 1);
    ASSERT_TRUE(rejected.is_err());
    EXPECT_EQ(rejected.unwrap_err().code, ZipError::INVALID_STATE);
}

TEST(ZipOutputStreamTest, PutNextEntryWithoutOpenReturnsErr) {
    ZipOutputStream zos;
    EXPECT_FALSE(zos.is_open());

    auto rejected = zos.put_next_entry(ZipEntry("orphan.txt", 0, 0, 0, 0, 0));
    ASSERT_TRUE(rejected.is_err());
    EXPECT_EQ(rejected.unwrap_err().code, ZipError::INVALID_STATE);
}

TEST(ZipOutputStreamTest, DoubleOpenReturnsErr) {
    const auto      outPath = temp_path("zos_double_open.zip");
    ZipOutputStream zos;
    EXPECT_TRUE(zos.open(outPath.string()).is_ok());
    EXPECT_TRUE(zos.is_open());

    auto again = zos.open(temp_path("zos_double_open_other.zip").string());
    ASSERT_TRUE(again.is_err());
    EXPECT_EQ(again.unwrap_err().code, ZipError::INVALID_STATE);
}

TEST(ZipOutputStreamTest, ExplicitCloseIsIdempotentAndResultChecked) {
    const auto      outPath = temp_path("zos_explicit_close.zip");
    ZipOutputStream zos;
    expect_ok(zos.open(outPath.string()), "open: ");
    expect_ok(zos.put_next_entry(ZipEntry("done.txt", 0, 0, 0, 0, 0)), "put: ");
    expect_ok(zos.close_entry(), "close entry: ");
    expect_ok(zos.close(), "close: ");
    EXPECT_FALSE(zos.is_open());
    // 已关闭后 close 是无害空操作。
    expect_ok(zos.close(), "second close: ");

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 1u);
}

}   // namespace
