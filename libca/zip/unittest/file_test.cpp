#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "libca/zip/entry.hpp"
#include "libca/zip/file.hpp"
#include "libca/zip/output_stream.hpp"
#include "zip_fixtures.hpp"

namespace {

using ca::zip::ZipError;
using ca::zip::ZipEntry;
using ca::zip::ZipFile;
using namespace zip_test;

TEST(ZipFileTest, ReadInlineMinimalZip) {
    auto    zip = build_minimal_stored_zip();
    ZipFile zf(zip);
    EXPECT_EQ(zf.size(), 1u);
    auto        data = expect_value(zf.read("a.txt"), "read a.txt: ");
    std::string content(data.begin(), data.end());
    EXPECT_EQ(content, "abc");
}

TEST(ZipFileTest, ReadPrefixedZip64Zip) {
    auto    zip = build_prefixed_zip64_zip();
    ZipFile zf(zip);
    EXPECT_EQ(zf.size(), 1u);

    const auto* entry = zf.get_entry("zip64.txt");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->relative_offset(), 6u);

    auto        bytes = expect_value(zf.read("zip64.txt"), "read zip64.txt: ");
    std::string content(bytes.begin(), bytes.end());
    EXPECT_EQ(content, "zip64");
}

TEST(ZipFileTest, NonExistentEntryReturnsErr) {
    auto    zip = build_minimal_stored_zip();
    ZipFile zf(zip);
    EXPECT_EQ(zf.get_entry("no_such_file.txt"), nullptr);
    auto missing = zf.read("no_such_file.txt");
    ASSERT_TRUE(missing.is_err());
    EXPECT_EQ(missing.unwrap_err().code, ZipError::NOT_FOUND);
}

TEST(ZipFileTest, ListEntries) {
    auto writerZip = temp_path("zf_list.zip");
    {
        ca::zip::ZipOutputStream zos(writerZip.string());
        expect_ok(zos.put_next_entry(ZipEntry("META-INF/MANIFEST.MF", 0, 0, 0, 0, 0)), "put META-INF: ");
        expect_ok(zos.write(std::vector<ca::u8>{'M', 'a', 'n', 'i'}));
        expect_ok(zos.close_entry(), "close META-INF: ");
        expect_ok(zos.put_next_entry(ZipEntry("Hello.class", 0, 4, 8, 0, 0)), "put Hello: ");
        expect_ok(zos.write(std::vector<ca::u8>{0xCA, 0xFE, 0xBA, 0xBE}));
        expect_ok(zos.close_entry(), "close Hello: ");
    }

    ZipFile zf(writerZip.string());
    auto    names = zf.entries();
    EXPECT_EQ(names.size(), 2u);
    EXPECT_NE(std::find(names.begin(), names.end(), "META-INF/MANIFEST.MF"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Hello.class"), names.end());
}

TEST(ZipFileTest, Crc32MismatchDetection) {
    auto    zip = build_crc_mismatch_zip();
    ZipFile zf(zip);
    auto*   entry = zf.get_entry("a.txt");
    ASSERT_NE(entry, nullptr);
    auto corrupted = zf.read("a.txt");
    ASSERT_TRUE(corrupted.is_err());
    EXPECT_EQ(corrupted.unwrap_err().code, ZipError::CRC_MISMATCH);
}

TEST(ZipFileTest, ReadMultipleGeneratedSamples) {
    // 多样本批量读取：stored 与 deflated 混合，含目录条目（空数据合法）。
    struct Sample {
        std::string file;
        size_t      entryCount;
    };

    auto dirSample = temp_path("zf_dir_sample.zip");
    {
        ca::zip::ZipOutputStream zos(dirSample.string());
        expect_ok(zos.put_next_entry(ZipEntry("dir/", 0, 0, 0, 0, 0)), "put dir/: ");
        expect_ok(zos.close_entry(), "close dir/: ");
        expect_ok(zos.put_next_entry(ZipEntry("dir/hello.txt", 0, 5, 8, 0, 0)), "put hello: ");
        expect_ok(zos.write(std::vector<ca::u8>{'H', 'e', 'l', 'l', 'o'}));
        expect_ok(zos.close_entry(), "close hello: ");
        expect_ok(zos.put_next_entry(ZipEntry("plain.txt", 0, 3, 0, 0, 0)), "put plain: ");
        expect_ok(zos.write(std::vector<ca::u8>{'p', 'l', 'a'}));
        expect_ok(zos.close_entry(), "close plain: ");
    }
    auto secretSample = temp_path("zf_secret_sample.zip");
    {
        const std::string        manifest = "Manifest-Version: 1.0\r\n";
        ca::zip::ZipOutputStream zos(secretSample.string());
        expect_ok(
            zos.put_next_entry(ZipEntry("META-INF/MANIFEST.MF", 0, static_cast<ca::u32>(manifest.size()), 0, 0, 0)),
            "put manifest: ");
        expect_ok(zos.write(std::vector<ca::u8>(manifest.begin(), manifest.end())));
        expect_ok(zos.close_entry(), "close manifest: ");
        expect_ok(zos.put_next_entry(ZipEntry("Secret.txt", 0, 6, 8, 0, 0)), "put secret: ");
        expect_ok(zos.write(std::vector<ca::u8>{'s', 'e', 'c', 'r', 'e', 't'}));
        expect_ok(zos.close_entry(), "close secret: ");
    }

    Sample samples[] = {
        {dirSample.string().c_str(), 3},
        {secretSample.string().c_str(), 2},
    };
    for (const auto& s : samples) {
        ZipFile zf(s.file);
        EXPECT_EQ(zf.size(), s.entryCount) << "Failed for " << s.file;
        auto names = zf.entries();
        for (const auto& name : names) {
            // 目录条目（'/' 结尾）合法地没有数据。
            if (!name.empty() && name.back() == '/')
                continue;
            EXPECT_FALSE(expect_value(zf.read(name), "read: ").empty())
                << "Empty data for " << name << " in " << s.file;
        }
    }
}

TEST(ZipFileTest, TruncatedArchivesReturnErr) {
    // 截断档：open 返回 Err 并保持关闭状态。
    auto full = build_minimal_stored_zip();

    std::vector<const std::vector<ca::u8>*> badInputs;
    const std::vector<ca::u8>               truncated(full.begin(), full.begin() + full.size() - 10);
    badInputs.push_back(&truncated);

    for (const auto* bytes : badInputs) {
        ZipFile zf;
        auto    opened = zf.open(*bytes);
        ASSERT_TRUE(opened.is_err()) << "Expected error for truncated archive";
        EXPECT_EQ(opened.unwrap_err().code, ZipError::INVALID_FORMAT);
        EXPECT_FALSE(zf.is_open());
    }
}

TEST(ZipFileTest, EntryMetadata) {
    const std::string manifest = "Manifest-Version: 1.0\r\n";
    auto              sample   = temp_path("zf_metadata.zip");
    {
        ca::zip::ZipOutputStream zos(sample.string());
        expect_ok(zos.put_next_entry(ZipEntry("Hello.class", 0, 4, 8, 0, 0)), "put Hello: ");
        expect_ok(zos.write(std::vector<ca::u8>{0xCA, 0xFE, 0xBA, 0xBE}));
        expect_ok(zos.close_entry(), "close Hello: ");
        expect_ok(
            zos.put_next_entry(ZipEntry("META-INF/MANIFEST.MF", 0, static_cast<ca::u32>(manifest.size()), 0, 0, 0)),
            "put manifest: ");
        expect_ok(zos.write(std::vector<ca::u8>(manifest.begin(), manifest.end())));
        expect_ok(zos.close_entry(), "close manifest: ");
    }

    ZipFile     zf(sample.string());
    const auto* hello = zf.get_entry("Hello.class");
    ASSERT_NE(hello, nullptr);
    EXPECT_EQ(hello->compression_method(), 8u);
    EXPECT_TRUE(hello->is_deflated());
    EXPECT_FALSE(hello->is_stored());

    const auto* man = zf.get_entry("META-INF/MANIFEST.MF");
    ASSERT_NE(man, nullptr);
    EXPECT_EQ(man->compression_method(), 0u);
    EXPECT_EQ(man->uncompressed_size(), manifest.size());
    EXPECT_TRUE(man->is_stored());
    EXPECT_FALSE(man->is_deflated());
}

TEST(ZipFileTest, CloseReopen) {
    auto    zip = build_minimal_stored_zip();
    ZipFile zf(zip);
    EXPECT_EQ(zf.size(), 1u);
    EXPECT_TRUE(zf.is_open());

    zf.close();
    EXPECT_FALSE(zf.is_open());
    EXPECT_EQ(zf.size(), 0u);
    EXPECT_EQ(zf.get_entry("a.txt"), nullptr);
    EXPECT_TRUE(zf.read("a.txt").is_err());

    expect_ok(zf.open(zip), "reopen: ");
    EXPECT_TRUE(zf.is_open());
    EXPECT_EQ(zf.size(), 1u);
    auto data = expect_value(zf.read("a.txt"), "read after reopen: ");
    EXPECT_EQ(data.size(), 3u);
}

TEST(ZipFileTest, EmptyDataReturnsErr) {
    std::vector<ca::u8> empty;
    ZipFile             zf;
    auto                opened = zf.open(empty);
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err().code, ZipError::INVALID_FORMAT);
    EXPECT_FALSE(zf.is_open());
}

TEST(ZipFileTest, WrongSignatureReturnsErr) {
    std::vector<ca::u8> garbage = {'N', 'O', 'T', 'A', 'Z', 'I', 'P'};
    ZipFile             zf;
    auto                opened = zf.open(garbage);
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err().code, ZipError::INVALID_FORMAT);
}

TEST(ZipFileTest, OpenMissingFileReturnsNotFound) {
    ZipFile zf;
    auto    opened = zf.open(temp_path("zf_no_such_archive.zip").string());
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err().code, ZipError::NOT_FOUND);
}

TEST(ZipFileTest, GetEntryCaseSensitivity) {
    auto    zip = build_minimal_stored_zip("MixedCase.TXT");
    ZipFile zf(zip);
    EXPECT_NE(zf.get_entry("MixedCase.TXT"), nullptr);
    EXPECT_EQ(zf.get_entry("mixedcase.txt"), nullptr);
    EXPECT_EQ(zf.get_entry("MIXEDCASE.TXT"), nullptr);
}

}   // namespace
