#include <gtest/gtest.h>

#include <algorithm>
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
using ca::zip::ZipFile;
using ca::zip::ZipOutputStream;
using namespace zip_test;

TEST(ZipOutputStreamTest, WriteReadStored)
{
    const auto outPath = temp_path("zos_stored.zip");
    {
        ZipOutputStream zos(outPath.string());
        zos.put_next_entry(ZipEntry("test.txt", 0, 5, 0, 0, 0));
        const std::string content = "Hello";
        zos.write(reinterpret_cast<const ca::u8*>(content.data()), content.size());
        zos.close_entry();
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 1u);
    auto        data = zf.read("test.txt");
    std::string result(data.begin(), data.end());
    EXPECT_EQ(result, "Hello");
}

TEST(ZipOutputStreamTest, WriteReadDeflated)
{
    const auto outPath = temp_path("zos_deflated.zip");
    {
        ZipOutputStream zos(outPath.string());
        zos.put_next_entry(ZipEntry("data.bin", 0, 1000, 8, 0, 0));
        std::vector<ca::u8> data(1000);
        for (int i = 0; i < 1000; i++)
            data[i] = static_cast<ca::u8>(i % 256);
        zos.write(data);
        zos.close_entry();
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 1u);
    auto result = zf.read("data.bin");
    EXPECT_EQ(result.size(), 1000u);
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(result[i], static_cast<ca::u8>(i % 256));
    }
}

TEST(ZipOutputStreamTest, MultipleEntries)
{
    const auto outPath = temp_path("zos_multi.zip");
    {
        ZipOutputStream zos(outPath.string());

        zos.put_next_entry(ZipEntry("a.txt", 0, 3, 0, 0, 0));
        zos.write(std::vector<ca::u8>{'a', 'b', 'c'});
        zos.close_entry();

        zos.put_next_entry(ZipEntry("b.txt", 0, 3, 8, 0, 0));
        zos.write(std::vector<ca::u8>{'d', 'e', 'f'});
        zos.close_entry();
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 2u);
    EXPECT_EQ(zf.read("a.txt"), std::vector<ca::u8>({'a', 'b', 'c'}));
    EXPECT_EQ(zf.read("b.txt"), std::vector<ca::u8>({'d', 'e', 'f'}));
}

TEST(ZipOutputStreamTest, EmptyEntry)
{
    const auto outPath = temp_path("zos_empty.zip");
    {
        ZipOutputStream zos(outPath.string());
        zos.put_next_entry(ZipEntry("empty.txt", 0, 0, 0, 0, 0));
        zos.close_entry();
    }

    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), 1u);
    EXPECT_TRUE(zf.read("empty.txt").empty());
}

TEST(ZipOutputStreamTest, DeflatedDataIntegrity)
{
    const auto outPath = temp_path("zos_integrity.zip");

    std::string original;
    for (int i = 0; i < 10000; i++) {
        original += "The quick brown fox jumps over the lazy dog. ";
    }

    {
        ZipOutputStream zos(outPath.string());
        zos.put_next_entry(
            ZipEntry("large.txt", 0, static_cast<ca::u32>(original.size()), 8, 0, 0));
        zos.write(reinterpret_cast<const ca::u8*>(original.data()), original.size());
        zos.close_entry();
    }

    ZipFile     zf(outPath.string());
    auto        data = zf.read("large.txt");
    std::string result(data.begin(), data.end());
    EXPECT_EQ(result.size(), original.size());
    EXPECT_EQ(result, original);
}

TEST(ZipOutputStreamTest, CompressionLevels)
{
    const std::string data = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

    for (const int level : {0, 1, 6, 9}) {
        const auto outPath = temp_path("zos_level" + std::to_string(level) + ".zip");
        {
            ZipOutputStream zos(outPath.string());
            zos.set_level(level);
            zos.put_next_entry(ZipEntry("test.txt", 0, static_cast<ca::u32>(data.size()), 8, 0, 0));
            zos.write(reinterpret_cast<const ca::u8*>(data.data()), data.size());
            zos.close_entry();
        }
        ZipFile     zf(outPath.string());
        auto        result = zf.read("test.txt");
        std::string content(result.begin(), result.end());
        EXPECT_EQ(content, data) << "Failed for compression level " << level;
    }
}

TEST(ZipOutputStreamTest, ManyEntries)
{
    const auto    outPath     = temp_path("zos_many_entries.zip");
    constexpr int kEntryCount = 50;
    {
        ZipOutputStream zos(outPath.string());
        for (int i = 0; i < kEntryCount; i++) {
            const std::string name = "file" + std::to_string(i) + ".txt";
            zos.put_next_entry(ZipEntry(name, 0, 4, 0, 0, 0));
            const std::string content = std::to_string(i);
            zos.write(reinterpret_cast<const ca::u8*>(content.data()), content.size());
            zos.close_entry();
        }
    }
    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), static_cast<size_t>(kEntryCount));
    auto names = zf.entries();
    EXPECT_EQ(names.size(), static_cast<size_t>(kEntryCount));
    for (int i = 0; i < kEntryCount; i++) {
        const std::string expectedName = "file" + std::to_string(i) + ".txt";
        EXPECT_NE(std::find(names.begin(), names.end(), expectedName), names.end());
        auto        data = zf.read(expectedName);
        std::string actual(data.begin(), data.end());
        EXPECT_EQ(actual, std::to_string(i));
    }
}

TEST(ZipOutputStreamTest, SpecialCharacters)
{
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
            zos.put_next_entry(ZipEntry(name, 0, static_cast<ca::u32>(name.size()), 0, 0, 0));
            zos.write(reinterpret_cast<const ca::u8*>(name.data()), name.size());
            zos.close_entry();
        }
    }
    ZipFile zf(outPath.string());
    EXPECT_EQ(zf.size(), names.size());
    for (const auto& name : names) {
        auto        data = zf.read(name);
        std::string actual(data.begin(), data.end());
        EXPECT_EQ(actual, name) << "Failed for entry: " << name;
    }
}

TEST(ZipOutputStreamTest, RoundtripMetadata)
{
    const auto          outPath = temp_path("zos_metadata.zip");
    std::vector<ca::u8> data    = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    const ca::u32       expectedCrc = ::crc32(0, data.data(), static_cast<uInt>(data.size()));
    {
        ZipOutputStream zos(outPath.string());
        zos.put_next_entry(ZipEntry("hello.dat", 0, static_cast<ca::u32>(data.size()), 8, 0, 0));
        zos.write(data);
        zos.close_entry();
    }
    auto        fileData = read_file_bytes(outPath);
    ZipFile     zf(fileData);
    const auto* entry = zf.get_entry("hello.dat");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->crc32(), expectedCrc);
    EXPECT_EQ(entry->uncompressed_size(), static_cast<ca::u32>(data.size()));
}

TEST(ZipOutputStreamTest, InvalidLevelThrows)
{
    ZipOutputStream zos(temp_path("zos_bad_level.zip").string());
    EXPECT_THROW(zos.set_level(10), std::runtime_error);
    EXPECT_THROW(zos.set_level(-2), std::runtime_error);
}

TEST(ZipOutputStreamTest, WriteWithoutEntryThrows)
{
    ZipOutputStream zos(temp_path("zos_no_entry.zip").string());
    const ca::u8    byte = 'x';
    EXPECT_THROW(zos.write(&byte, 1), std::runtime_error);
}

}   // namespace
