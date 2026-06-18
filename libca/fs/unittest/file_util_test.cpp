#include <gmock/gmock.h>

#include <cstdio>
#include <fstream>

#include "libca/fs/file_util.hpp"

namespace ca { namespace fs { namespace test {

using namespace testing;

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
    ByteVector data = {0x00, 0xFF, 0xAB, 0xCD, 0x12, 0x34};

    EXPECT_TRUE(FileUtil::write_bytes(filePath, data));

    auto result = FileUtil::read_all_bytes(filePath);
    ASSERT_TRUE(result.is_ok()) << "readAllBytes failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap(), data);
}

TEST(FileUtilTest, ReadAllText)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("hello.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "Hello, 世界!"));

    auto result = FileUtil::read_all_text(filePath);
    ASSERT_TRUE(result.is_ok()) << "readAllText failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap(), "Hello, 世界!");
}

TEST(FileUtilTest, ReadAllBytes_FileNotFound)
{
    auto result = FileUtil::read_all_bytes("/nonexistent/path/file.bin");
    EXPECT_TRUE(result.is_err());
}

TEST(FileUtilTest, ReadAllBytes_PathIsDirectory)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto result = FileUtil::read_all_bytes(tmp.path());
    EXPECT_TRUE(result.is_err());
}

TEST(FileUtilTest, ReadAllText_FileNotFound)
{
    auto result = FileUtil::read_all_text("/nonexistent/path/file.txt");
    EXPECT_TRUE(result.is_err());
}

// ==================== writeText / writeBytes ====================

TEST(FileUtilTest, WriteBytes_OverwriteDefault)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("overwrite.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "first"));
    EXPECT_TRUE(FileUtil::write_text(filePath, "second"));

    auto result = FileUtil::read_all_text(filePath);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), "second");
}

TEST(FileUtilTest, WriteBytes_Append)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("append.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "hello", FileMode::OVERWRITE));
    EXPECT_TRUE(FileUtil::write_text(filePath, " world", FileMode::APPEND));

    auto result = FileUtil::read_all_text(filePath);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), "hello world");
}

TEST(FileUtilTest, WriteBytes_CreateNew_Success)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("create_new.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "new file", FileMode::CREATE_NEW));
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, WriteBytes_CreateNew_Existing_Fails)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("already_exists.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "original", FileMode::CREATE_NEW));
    EXPECT_FALSE(FileUtil::write_text(filePath, "again", FileMode::CREATE_NEW));
}

TEST(FileUtilTest, WriteBytes_CreatesParentDirectories)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("subdir/nested/file.txt");
    EXPECT_TRUE(FileUtil::write_text(filePath, "nested"));
    EXPECT_TRUE(FileUtil::exists(filePath));
}

// ==================== getSize ====================

TEST(FileUtilTest, GetSize)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.make_path("size_test.bin");
    ByteVector data(4096, 0xAB);
    EXPECT_TRUE(FileUtil::write_bytes(filePath, data));

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

// ==================== listFiles / listEntries ====================

TEST(FileUtilTest, ListFiles_Flat)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.create_file("a.txt");
    tmp.create_file("b.txt");

    auto result = FileUtil::list_files(tmp.path(), false);
    ASSERT_TRUE(result.is_ok()) << "listFiles failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap().size(), 2u);
}

TEST(FileUtilTest, ListFiles_Recursive)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.create_file("a.txt");
    tmp.create_file("sub/b.txt");

    auto result = FileUtil::list_files(tmp.path(), true);
    ASSERT_TRUE(result.is_ok()) << "listFiles recursive failed: " << result.unwrap_err();
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
    ASSERT_TRUE(result.is_ok()) << "listEntries failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap().size(), 3u);
}

// ==================== copy ====================

TEST(FileUtilTest, Copy_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.make_path("src.txt");
    auto dst = tmp.make_path("dst.txt");
    EXPECT_TRUE(FileUtil::write_text(src, "copy test"));
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
    EXPECT_TRUE(FileUtil::write_text(tmp.make_path("srcdir/a.txt"), "file in dir"));

    EXPECT_TRUE(FileUtil::copy(srcDir, dstDir));
    EXPECT_TRUE(FileUtil::is_directory(dstDir));
}

// ==================== move ====================

TEST(FileUtilTest, Move_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.make_path("move_src.txt");
    auto dst = tmp.make_path("move_dst.txt");
    EXPECT_TRUE(FileUtil::write_text(src, "move test"));
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
    EXPECT_TRUE(FileUtil::write_text(src, "important data"));

    auto result = FileUtil::backup(src);
    ASSERT_TRUE(result.is_ok()) << "backup failed: " << result.unwrap_err();

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
    ASSERT_TRUE(result.is_ok()) << "createTempFile failed: " << result.unwrap_err();
    EXPECT_TRUE(FileUtil::exists(result.unwrap()));
    EXPECT_TRUE(FileUtil::is_file(result.unwrap()));
    FileUtil::remove(result.unwrap());
}

TEST(FileUtilTest, CreateTempDirectory)
{
    auto result = FileUtil::create_temp_directory("libca_dir_");
    ASSERT_TRUE(result.is_ok()) << "createTempDirectory failed: " << result.unwrap_err();
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
