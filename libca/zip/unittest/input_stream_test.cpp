#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "libca/zip/entry.hpp"
#include "libca/zip/input_stream.hpp"
#include "libca/zip/output_stream.hpp"
#include "zip_fixtures.hpp"

namespace {

using ca::zip::ZipEntry;
using ca::zip::ZipError;
using ca::zip::ZipInputStream;
using ca::zip::ZipOutputStream;
using zip_test::expect_ok;
using zip_test::expect_value;
using namespace zip_test;

TEST(ZipInputStreamTest, ReadStoredEntry) {
    const auto outPath = temp_path("zis_stored.zip");
    {
        ZipOutputStream zos(outPath.string());
        expect_ok(zos.put_next_entry(ZipEntry("hello.txt", 0, 5, 0, 0, 0)), "put: ");
        const std::string content = "Hello";
        expect_ok(zos.write(reinterpret_cast<const ca::u8*>(content.data()), content.size()), "write: ");
        expect_ok(zos.close_entry(), "close: ");
    }

    auto           fileData = read_file_bytes(outPath);
    ZipInputStream zis(std::move(fileData));

    auto entry = expect_value(zis.get_next_entry(), "next entry: ");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->name(), "hello.txt");
    EXPECT_EQ(entry->compression_method(), 0);

    auto        data = expect_value(zis.read_all(), "read_all: ");
    std::string result(data.begin(), data.end());
    EXPECT_EQ(result, "Hello");
}

TEST(ZipInputStreamTest, ReadDeflatedEntry) {
    const auto outPath = temp_path("zis_deflated.zip");
    {
        ZipOutputStream zos(outPath.string());
        expect_ok(zos.put_next_entry(ZipEntry("data.bin", 0, 1000, 8, 0, 0)), "put: ");
        std::vector<ca::u8> data(1000);
        for (int i = 0; i < 1000; i++)
            data[i] = static_cast<ca::u8>(i % 256);
        expect_ok(zos.write(data), "write: ");
        expect_ok(zos.close_entry(), "close: ");
    }

    auto           fileData = read_file_bytes(outPath);
    ZipInputStream zis(std::move(fileData));

    auto entry = expect_value(zis.get_next_entry(), "next entry: ");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->name(), "data.bin");
    EXPECT_TRUE(entry->is_deflated());

    auto result = expect_value(zis.read_all(), "read_all: ");
    EXPECT_EQ(result.size(), 1000u);
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(result[i], static_cast<ca::u8>(i % 256));
    }
}

TEST(ZipInputStreamTest, MultipleEntries) {
    const auto outPath = temp_path("zis_multi.zip");
    {
        ZipOutputStream zos(outPath.string());

        expect_ok(zos.put_next_entry(ZipEntry("a.txt", 0, 3, 0, 0, 0)), "put a: ");
        expect_ok(zos.write(std::vector<ca::u8>{'a', 'b', 'c'}), "write a: ");
        expect_ok(zos.close_entry(), "close a: ");

        expect_ok(zos.put_next_entry(ZipEntry("b.txt", 0, 3, 8, 0, 0)), "put b: ");
        expect_ok(zos.write(std::vector<ca::u8>{'d', 'e', 'f'}), "write b: ");
        expect_ok(zos.close_entry(), "close b: ");
    }

    auto           fileData = read_file_bytes(outPath);
    ZipInputStream zis(std::move(fileData));

    {
        auto e1 = expect_value(zis.get_next_entry(), "entry 1: ");
        ASSERT_NE(e1, nullptr);
        EXPECT_EQ(e1->name(), "a.txt");
        EXPECT_TRUE(e1->is_stored());
        auto d1 = expect_value(zis.read_all(), "read a: ");
        EXPECT_EQ(d1, std::vector<ca::u8>({'a', 'b', 'c'}));
    }

    {
        auto e2 = expect_value(zis.get_next_entry(), "entry 2: ");
        ASSERT_NE(e2, nullptr);
        EXPECT_EQ(e2->name(), "b.txt");
        EXPECT_TRUE(e2->is_deflated());
        auto d2 = expect_value(zis.read_all(), "read b: ");
        EXPECT_EQ(d2, std::vector<ca::u8>({'d', 'e', 'f'}));
    }

    EXPECT_EQ(expect_value(zis.get_next_entry(), "exhausted: "), nullptr);
}

TEST(ZipInputStreamTest, ReadStoredEntryWithUnsignedDataDescriptor) {
    auto           fileData = build_stored_zip_with_unsigned_data_descriptor();
    ZipInputStream zis(std::move(fileData));

    auto entry = expect_value(zis.get_next_entry(), "next entry: ");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->name(), "a.txt");
    EXPECT_TRUE(entry->is_stored());

    auto data = expect_value(zis.read_all(), "read_all: ");
    EXPECT_EQ(data, std::vector<ca::u8>({'a', 'b', 'c'}));
    EXPECT_EQ(expect_value(zis.get_next_entry(), "exhausted: "), nullptr);
}

TEST(ZipInputStreamTest, EmptyStoredEntryDoesNotConsumeFollowingHeader) {
    auto           fileData = build_two_local_headers_with_empty_first_entry();
    ZipInputStream zis(std::move(fileData));

    auto first = expect_value(zis.get_next_entry(), "first entry: ");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->name(), "empty.txt");
    EXPECT_TRUE(expect_value(zis.read_all(), "read empty: ").empty());

    auto second = expect_value(zis.get_next_entry(), "second entry: ");
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->name(), "b.txt");
    EXPECT_EQ(expect_value(zis.read_all(), "read b: "), std::vector<ca::u8>({'b', 'c', 'd'}));
    EXPECT_EQ(expect_value(zis.get_next_entry(), "exhausted: "), nullptr);
}

TEST(ZipInputStreamTest, TruncatedUnsignedDataDescriptorReturnsErr) {
    auto           fileData = build_truncated_unsigned_data_descriptor_zip();
    ZipInputStream zis(std::move(fileData));

    auto broken = zis.get_next_entry();
    ASSERT_TRUE(broken.is_err());
    EXPECT_EQ(broken.unwrap_err().code, ZipError::INVALID_FORMAT);
}

}   // namespace
