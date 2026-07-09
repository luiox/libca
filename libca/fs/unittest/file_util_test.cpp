#include <gmock/gmock.h>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>

#include "libca/fs/file_util.hpp"

namespace ca { namespace fs { namespace test {

using namespace testing;

/// 测试辅助：比较 ca::core::Bytes 与裸字节序列内容是否一致。
/// Bytes 无 operator==，这里取其底层指针与期望字节做 memcmp。
::testing::AssertionResult bytes_match(const ca::core::Bytes& actual, const ca::u8* expected,
                                       ca::usize expected_len)
{
    if (actual.len() != expected_len) {
        return ::testing::AssertionFailure()
            << "length mismatch: actual=" << actual.len() << " expected=" << expected_len;
    }
    if (expected_len > 0 &&
        std::memcmp(actual.as_ptr(), expected, expected_len) != 0) {
        return ::testing::AssertionFailure() << "content mismatch";
    }
    return ::testing::AssertionSuccess();
}

/// 测试辅助：临时目录 RAII 守卫
class TempDirGuard
{
public:
    TempDirGuard()
    {
        auto result = FileUtil::create_temp_directory("libca_fs_test_");
        if (result.is_ok()) {
            m_path = std::move(result.unwrap());
        }
    }

    ~TempDirGuard()
    {
        if (!m_path.empty()) {
            FileUtil::remove_all(m_path);
        }
    }

    const std::string& path() const { return m_path; }
    bool valid() const { return !m_path.empty(); }

    std::string make_path(const std::string& relative) const
    {
        return PathUtil::join(m_path, relative);
    }

    void create_file(const std::string& relative, const std::string& content = "") const
    {
        auto full = make_path(relative);
        FileUtil::write_text(full, content);
    }

private:
    std::string m_path;
};

// ==================== readAllBytes / readAllText ====================

TEST(FileUtilTest, ReadWriteRoundtrip)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("test_roundtrip.bin");
    static const ca::u8 data[] = {0x00, 0xFF, 0xAB, 0xCD, 0x12, 0x34};
    ca::core::ByteSlice slice(data, sizeof(data));

    EXPECT_TRUE(FileUtil::write_bytes(filePath, slice).is_ok());

    auto result = FileUtil::read_all_bytes(filePath);
    ASSERT_TRUE(result.is_ok()) << "readAllBytes failed: " << to_string(result.unwrap_err());
    EXPECT_TRUE(bytes_match(result.unwrap(), data, sizeof(data)));
}

TEST(FileUtilTest, ReadAllBytes_ReturnsBytes)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("bytes_type.bin");
    static const ca::u8 data[] = {0x01, 0x02, 0x03, 0x04};
    ca::core::ByteSlice slice(data, sizeof(data));
    ASSERT_TRUE(FileUtil::write_bytes(filePath, slice).is_ok());

    auto result = FileUtil::read_all_bytes(filePath);
    ASSERT_TRUE(result.is_ok());
    auto bytes = result.unwrap();
    // Bytes 是不可变共享存储，类型即为 ca::core::Bytes，长度与内容正确。
    EXPECT_EQ(bytes.len(), 4u);
    EXPECT_EQ(bytes.as_ptr()[0], 0x01);
    EXPECT_EQ(bytes.as_ptr()[3], 0x04);
}

TEST(FileUtilTest, ReadAllText)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("hello.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "Hello, 世界!").is_ok());

    auto result = FileUtil::read_all_text(filePath);
    ASSERT_TRUE(result.is_ok()) << "readAllText failed: " << to_string(result.unwrap_err());
    EXPECT_EQ(result.unwrap(), "Hello, 世界!");
}

TEST(FileUtilTest, ReadAllBytes_FileNotFound)
{
    auto result = FileUtil::read_all_bytes("/nonexistent/path/file.bin");
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err(), FsError::FileNotFound);
}

TEST(FileUtilTest, ReadAllBytes_PathIsDirectory)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto result = FileUtil::read_all_bytes(tmp.path());
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err(), FsError::NotARegularFile);
}

TEST(FileUtilTest, ReadAllText_FileNotFound)
{
    auto result = FileUtil::read_all_text("/nonexistent/path/file.txt");
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err(), FsError::FileNotFound);
}

// ==================== writeText / writeBytes ====================

TEST(FileUtilTest, WriteBytes_OverwriteDefault)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("overwrite.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "first").is_ok());
    EXPECT_TRUE(FileUtil::write_text(filePath, "second").is_ok());

    auto result = FileUtil::read_all_text(filePath);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), "second");
}

TEST(FileUtilTest, WriteBytes_Append)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("append.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "hello", FileMode::OVERWRITE).is_ok());
    EXPECT_TRUE(FileUtil::write_text(filePath, " world", FileMode::APPEND).is_ok());

    auto result = FileUtil::read_all_text(filePath);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), "hello world");
}

TEST(FileUtilTest, WriteBytes_CreateNew_Success)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("create_new.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "new file", FileMode::CREATE_NEW).is_ok());
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, WriteBytes_CreateNew_Existing_Fails)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("already_exists.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "original", FileMode::CREATE_NEW).is_ok());
    auto result = FileUtil::write_text(filePath, "again", FileMode::CREATE_NEW);
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err(), FsError::AlreadyExists);
}

TEST(FileUtilTest, WriteBytes_CreatesParentDirectories)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("subdir/nested/file.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "nested").is_ok());
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, AtomicWriteText_ReplacesContent)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("atomic.txt");
    ASSERT_TRUE(FileUtil::write_text(filePath, "old").is_ok());
    ASSERT_TRUE(FileUtil::atomic_write_text(filePath, "new content").is_ok());

    auto content = FileUtil::read_all_text(filePath);
    ASSERT_TRUE(content.is_ok());
    EXPECT_EQ(content.unwrap(), "new content");
}

TEST(FileUtilTest, AtomicWriteText_RenameFailurePreservesExistingTarget)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto dirPath = tmp.make_path("atomic_target_dir");
    ASSERT_TRUE(FileUtil::create_directories(dirPath));

    auto result = FileUtil::atomic_write_text(dirPath, "should not replace directory");
    ASSERT_TRUE(result.is_err());
    EXPECT_TRUE(FileUtil::is_directory(dirPath));

    auto entries = FileUtil::list_entries(tmp.path());
    ASSERT_TRUE(entries.is_ok());
    for (const auto& entry : entries.unwrap()) {
        EXPECT_THAT(entry, Not(HasSubstr("atomic_target_dir.tmp.")));
    }
}

TEST(FileUtilTest, ReadLines_StripsLineEndings)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("lines.txt");
    ASSERT_TRUE(FileUtil::write_text(filePath, "a\r\nb\nc").is_ok());

    auto lines = FileUtil::read_lines(filePath);
    ASSERT_TRUE(lines.is_ok());
    ASSERT_EQ(lines.unwrap().size(), 3u);
    EXPECT_EQ(lines.unwrap()[0], "a");
    EXPECT_EQ(lines.unwrap()[1], "b");
    EXPECT_EQ(lines.unwrap()[2], "c");
}

// ==================== getSize ====================

TEST(FileUtilTest, GetSize)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("size_test.bin");
    std::vector<ca::u8> data(4096, 0xAB);
    ca::core::ByteSlice slice(data.data(), data.size());
    EXPECT_TRUE(FileUtil::write_bytes(filePath, slice).is_ok());

    EXPECT_EQ(FileUtil::size(filePath), 4096);
}

TEST(FileUtilTest, GetSize_FileNotFound)
{
    EXPECT_EQ(FileUtil::size("/nonexistent"), -1);
}

TEST(FileUtilTest, GetSize_EmptyFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("empty.txt");
    FileUtil::create_file(filePath);

    EXPECT_EQ(FileUtil::size(filePath), 0);
}

// ==================== exists / isFile / isDirectory ====================

TEST(FileUtilTest, Exists_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("exists_test.txt");
    EXPECT_FALSE(FileUtil::exists(filePath));

    FileUtil::create_file(filePath);
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, IsFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("is_file_test.txt");
    FileUtil::create_file(filePath);

    EXPECT_TRUE(FileUtil::is_file(filePath));
    EXPECT_FALSE(FileUtil::is_directory(filePath));
}

TEST(FileUtilTest, IsDirectory)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    EXPECT_TRUE(FileUtil::is_directory(tmp.path()));
    EXPECT_FALSE(FileUtil::is_file(tmp.path()));
}

TEST(FileUtilTest, MetadataAndPermissions)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("meta.txt");
    ASSERT_TRUE(FileUtil::write_text(filePath, "metadata").is_ok());

    auto meta = FileUtil::metadata(filePath);
    ASSERT_TRUE(meta.is_ok());
    EXPECT_TRUE(meta.unwrap().exists);
    EXPECT_TRUE(meta.unwrap().is_file);
    EXPECT_FALSE(meta.unwrap().is_directory);
    EXPECT_EQ(meta.unwrap().size, 8);

    auto perms = FileUtil::permissions(filePath);
    ASSERT_TRUE(perms.is_ok());
    EXPECT_NE(perms.unwrap(), std::filesystem::perms::none);
}

// ==================== listFiles / listEntries ====================

TEST(FileUtilTest, ListFiles_Flat)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.create_file("a.txt");
    tmp.create_file("b.txt");

    auto result = FileUtil::list_files(tmp.path(), false);
    ASSERT_TRUE(result.is_ok()) << "listFiles failed: " << to_string(result.unwrap_err());
    EXPECT_EQ(result.unwrap().size(), 2u);
}

TEST(FileUtilTest, ListFiles_Recursive)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.create_file("a.txt");
    tmp.create_file("sub/b.txt");

    auto result = FileUtil::list_files(tmp.path(), true);
    ASSERT_TRUE(result.is_ok()) << "listFiles recursive failed: " << to_string(result.unwrap_err());
    EXPECT_EQ(result.unwrap().size(), 2u);
}

TEST(FileUtilTest, ListFiles_DirectoryNotFound)
{
    auto result = FileUtil::list_files("/nonexistent_dir");
    EXPECT_TRUE(result.is_err());
}

TEST(FileUtilTest, ListEntries)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.create_file("f1.txt");
    tmp.create_file("f2.txt");
    EXPECT_TRUE(FileUtil::create_directories(tmp.make_path("subdir")));

    auto result = FileUtil::list_entries(tmp.path());
    ASSERT_TRUE(result.is_ok()) << "listEntries failed: " << to_string(result.unwrap_err());
    EXPECT_EQ(result.unwrap().size(), 3u);
}

// ==================== copy ====================

TEST(FileUtilTest, Copy_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.make_path("src.txt");
    auto dst = tmp.make_path("dst.txt");
    EXPECT_TRUE(FileUtil::write_text(src, "copy test").is_ok());
    EXPECT_TRUE(FileUtil::copy(src, dst));
    EXPECT_TRUE(FileUtil::exists(dst));

    auto content = FileUtil::read_all_text(dst);
    ASSERT_TRUE(content.is_ok());
    EXPECT_EQ(content.unwrap(), "copy test");
}

TEST(FileUtilTest, Copy_SourceNotFound)
{
    EXPECT_FALSE(FileUtil::copy("/nonexistent", "/tmp/nowhere"));
}

TEST(FileUtilTest, Copy_Directory)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto srcDir = tmp.make_path("srcdir");
    auto dstDir = tmp.make_path("dstdir");
    EXPECT_TRUE(FileUtil::create_directories(srcDir));
    EXPECT_TRUE(FileUtil::write_text(tmp.make_path("srcdir/a.txt"), "file in dir").is_ok());

    EXPECT_TRUE(FileUtil::copy(srcDir, dstDir));
    EXPECT_TRUE(FileUtil::is_directory(dstDir));
}

TEST(FileUtilTest, CopyDir_Recursive)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto srcDir = tmp.make_path("copy_src");
    auto dstDir = tmp.make_path("copy_dst");
    ASSERT_TRUE(FileUtil::create_directories(srcDir));
    ASSERT_TRUE(FileUtil::write_text(tmp.make_path("copy_src/a.txt"), "a").is_ok());
    ASSERT_TRUE(FileUtil::write_text(tmp.make_path("copy_src/nested/b.txt"), "b").is_ok());

    auto copied = FileUtil::copy_dir(srcDir, dstDir);
    ASSERT_TRUE(copied.is_ok());

    auto a = FileUtil::read_all_text(tmp.make_path("copy_dst/a.txt"));
    auto b = FileUtil::read_all_text(tmp.make_path("copy_dst/nested/b.txt"));
    ASSERT_TRUE(a.is_ok());
    ASSERT_TRUE(b.is_ok());
    EXPECT_EQ(a.unwrap(), "a");
    EXPECT_EQ(b.unwrap(), "b");
}

TEST(FileUtilTest, CopyDir_OverwritesBrokenSymlinkTargetWhenSupported)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto srcDir = std::filesystem::path(tmp.make_path("copy_symlink_src"));
    auto dstDir = std::filesystem::path(tmp.make_path("copy_symlink_dst"));
    ASSERT_TRUE(FileUtil::create_directories(srcDir.generic_string()));
    ASSERT_TRUE(FileUtil::create_directories(dstDir.generic_string()));

    auto srcLink = srcDir / "link.txt";
    std::error_code ec;
    std::filesystem::create_symlink("target.txt", srcLink, ec);
    if (ec) {
        GTEST_SKIP() << "symlink creation is not supported in this environment: " << ec.message();
    }

    auto dstLink = dstDir / "link.txt";
    std::filesystem::create_symlink("missing.txt", dstLink, ec);
    if (ec) {
        GTEST_SKIP() << "symlink creation is not supported in this environment: " << ec.message();
    }

    auto copied = FileUtil::copy_dir(srcDir.generic_string(), dstDir.generic_string(), true);
    ASSERT_TRUE(copied.is_ok()) << to_string(copied.unwrap_err());
    EXPECT_TRUE(std::filesystem::is_symlink(dstLink));
    EXPECT_EQ(std::filesystem::read_symlink(dstLink).generic_string(), "target.txt");
}

TEST(FileUtilTest, Glob_StarAndRecursive)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    ASSERT_TRUE(FileUtil::write_text(tmp.make_path("a.txt"), "a").is_ok());
    ASSERT_TRUE(FileUtil::write_text(tmp.make_path("b.log"), "b").is_ok());
    ASSERT_TRUE(FileUtil::write_text(tmp.make_path("nested/c.txt"), "c").is_ok());

    auto flat = FileUtil::glob(PathUtil::join(tmp.path(), "*.txt"));
    ASSERT_TRUE(flat.is_ok());
    ASSERT_EQ(flat.unwrap().size(), 1u);
    EXPECT_THAT(flat.unwrap()[0], HasSubstr("a.txt"));

    auto recursive = FileUtil::glob(PathUtil::join(tmp.path(), "**/*.txt"));
    ASSERT_TRUE(recursive.is_ok());
    ASSERT_EQ(recursive.unwrap().size(), 2u);
    EXPECT_THAT(recursive.unwrap()[0], HasSubstr("a.txt"));
    EXPECT_THAT(recursive.unwrap()[1], HasSubstr("nested/c.txt"));
}

TEST(FileUtilTest, Glob_DirectoryWildcardUsesRecursiveWalk)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    ASSERT_TRUE(FileUtil::write_text(tmp.make_path("nested/c.txt"), "c").is_ok());
    ASSERT_TRUE(FileUtil::write_text(tmp.make_path("other/d.txt"), "d").is_ok());

    auto matched = FileUtil::glob(PathUtil::join(tmp.path(), "*/c.txt"));
    ASSERT_TRUE(matched.is_ok()) << to_string(matched.unwrap_err());
    ASSERT_EQ(matched.unwrap().size(), 1u);
    EXPECT_THAT(matched.unwrap()[0], HasSubstr("nested/c.txt"));
}

// ==================== move ====================

TEST(FileUtilTest, Move_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.make_path("move_src.txt");
    auto dst = tmp.make_path("move_dst.txt");
    EXPECT_TRUE(FileUtil::write_text(src, "move test").is_ok());
    EXPECT_TRUE(FileUtil::move(src, dst));
    EXPECT_FALSE(FileUtil::exists(src));
    EXPECT_TRUE(FileUtil::exists(dst));
}

TEST(FileUtilTest, Move_SourceNotFound)
{
    EXPECT_FALSE(FileUtil::move("/nonexistent", "/tmp/nowhere"));
}

// ==================== remove / removeAll ====================

TEST(FileUtilTest, Remove_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("to_remove.txt");
    FileUtil::create_file(filePath);
    EXPECT_TRUE(FileUtil::exists(filePath));

    EXPECT_TRUE(FileUtil::remove(filePath));
    EXPECT_FALSE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, Remove_NonExistent)
{
    EXPECT_FALSE(FileUtil::remove("/nonexistent/file.txt"));
}

TEST(FileUtilTest, RemoveAll_Directory)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto dirPath = tmp.make_path("remove_subdir");
    EXPECT_TRUE(FileUtil::create_directories(dirPath));
    tmp.create_file("remove_subdir/a.txt");

    EXPECT_TRUE(FileUtil::remove_all(dirPath));
    EXPECT_FALSE(FileUtil::exists(dirPath));
}

// ==================== create_file / create_directories ====================

TEST(FileUtilTest, CreateFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("brand_new.txt");
    EXPECT_TRUE(FileUtil::create_file(filePath));
    EXPECT_TRUE(FileUtil::exists(filePath));
    EXPECT_TRUE(FileUtil::is_file(filePath));
    EXPECT_EQ(FileUtil::size(filePath), 0);
}

TEST(FileUtilTest, CreateFile_WithParentDirectories)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("a/b/c/deep.txt");
    EXPECT_TRUE(FileUtil::create_file(filePath));
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, CreateDirectories)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto dirPath = tmp.make_path("deeply/nested/dir/structure");
    EXPECT_TRUE(FileUtil::create_directories(dirPath));
    EXPECT_TRUE(FileUtil::is_directory(dirPath));
}

// ==================== backup ====================

TEST(FileUtilTest, Backup_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.make_path("backup_me.txt");
    EXPECT_TRUE(FileUtil::write_text(src, "important data").is_ok());

    auto result = FileUtil::backup(src);
    ASSERT_TRUE(result.is_ok()) << "backup failed: " << to_string(result.unwrap_err());

    EXPECT_TRUE(FileUtil::exists(result.unwrap()));
    EXPECT_NE(result.unwrap(), src);

    auto content = FileUtil::read_all_text(result.unwrap());
    ASSERT_TRUE(content.is_ok());
    EXPECT_EQ(content.unwrap(), "important data");
}

TEST(FileUtilTest, Backup_NonExistent)
{
    auto result = FileUtil::backup("/nonexistent/path");
    EXPECT_TRUE(result.is_err());
}

// ==================== createTempFile / createTempDirectory ====================

TEST(FileUtilTest, CreateTempFile)
{
    auto result = FileUtil::create_temp_file("libca_", ".tmp");
    ASSERT_TRUE(result.is_ok()) << "createTempFile failed: " << to_string(result.unwrap_err());
    EXPECT_TRUE(FileUtil::exists(result.unwrap()));
    EXPECT_TRUE(FileUtil::is_file(result.unwrap()));
    FileUtil::remove(result.unwrap());
}

TEST(FileUtilTest, CreateTempDirectory)
{
    auto result = FileUtil::create_temp_directory("libca_dir_");
    ASSERT_TRUE(result.is_ok()) << "createTempDirectory failed: " << to_string(result.unwrap_err());
    EXPECT_TRUE(FileUtil::is_directory(result.unwrap()));
    FileUtil::remove_all(result.unwrap());
}

// ==================== isReadable / isWritable ====================

TEST(FileUtilTest, IsReadable_ExistingFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("readable.txt");
    FileUtil::create_file(filePath);

    EXPECT_TRUE(FileUtil::is_readable(filePath));
    EXPECT_TRUE(FileUtil::is_writable(filePath));
}

TEST(FileUtilTest, IsReadable_NonExistent)
{
    EXPECT_FALSE(FileUtil::is_readable("/nonexistent/path"));
    EXPECT_FALSE(FileUtil::is_writable("/nonexistent/path"));
}

}}}  // namespace ca::fs::test
