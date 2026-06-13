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
        auto result = FileUtil::createTempDirectory("libca_fs_test_");
        if (result.is_ok()) {
            m_path = std::move(result.unwrap());
        }
    }

    ~TempDirGuard()
    {
        if (!m_path.empty()) {
            FileUtil::removeAll(m_path);
        }
    }

    const std::string& path() const { return m_path; }
    bool valid() const { return !m_path.empty(); }

    std::string makePath(const std::string& relative) const
    {
        return PathUtil::join(m_path, relative);
    }

    void createFile(const std::string& relative, const std::string& content = "") const
    {
        auto full = makePath(relative);
        FileUtil::writeText(full, content);
    }

private:
    std::string m_path;
};

// ==================== readAllBytes / readAllText ====================

TEST(FileUtilTest, ReadWriteRoundtrip)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("test_roundtrip.bin");
    ByteVector data = {0x00, 0xFF, 0xAB, 0xCD, 0x12, 0x34};

    EXPECT_TRUE(FileUtil::writeBytes(filePath, data));

    auto result = FileUtil::readAllBytes(filePath);
    ASSERT_TRUE(result.is_ok()) << "readAllBytes failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap(), data);
}

TEST(FileUtilTest, ReadAllText)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("hello.txt");
    EXPECT_TRUE(FileUtil::writeText(filePath, "Hello, 世界!"));

    auto result = FileUtil::readAllText(filePath);
    ASSERT_TRUE(result.is_ok()) << "readAllText failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap(), "Hello, 世界!");
}

TEST(FileUtilTest, ReadAllBytes_FileNotFound)
{
    auto result = FileUtil::readAllBytes("/nonexistent/path/file.bin");
    EXPECT_TRUE(result.is_err());
}

TEST(FileUtilTest, ReadAllBytes_PathIsDirectory)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto result = FileUtil::readAllBytes(tmp.path());
    EXPECT_TRUE(result.is_err());
}

TEST(FileUtilTest, ReadAllText_FileNotFound)
{
    auto result = FileUtil::readAllText("/nonexistent/path/file.txt");
    EXPECT_TRUE(result.is_err());
}

// ==================== writeText / writeBytes ====================

TEST(FileUtilTest, WriteBytes_OverwriteDefault)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("overwrite.txt");
    EXPECT_TRUE(FileUtil::writeText(filePath, "first"));
    EXPECT_TRUE(FileUtil::writeText(filePath, "second"));

    auto result = FileUtil::readAllText(filePath);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), "second");
}

TEST(FileUtilTest, WriteBytes_Append)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("append.txt");
    EXPECT_TRUE(FileUtil::writeText(filePath, "hello", FileMode::Overwrite));
    EXPECT_TRUE(FileUtil::writeText(filePath, " world", FileMode::Append));

    auto result = FileUtil::readAllText(filePath);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.unwrap(), "hello world");
}

TEST(FileUtilTest, WriteBytes_CreateNew_Success)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("create_new.txt");
    EXPECT_TRUE(FileUtil::writeText(filePath, "new file", FileMode::CreateNew));
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, WriteBytes_CreateNew_Existing_Fails)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("already_exists.txt");
    EXPECT_TRUE(FileUtil::writeText(filePath, "original", FileMode::CreateNew));
    EXPECT_FALSE(FileUtil::writeText(filePath, "again", FileMode::CreateNew));
}

TEST(FileUtilTest, WriteBytes_CreatesParentDirectories)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("subdir/nested/file.txt");
    EXPECT_TRUE(FileUtil::writeText(filePath, "nested"));
    EXPECT_TRUE(FileUtil::exists(filePath));
}

// ==================== getSize ====================

TEST(FileUtilTest, GetSize)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("size_test.bin");
    ByteVector data(4096, 0xAB);
    EXPECT_TRUE(FileUtil::writeBytes(filePath, data));

    EXPECT_EQ(FileUtil::getSize(filePath), 4096);
}

TEST(FileUtilTest, GetSize_FileNotFound)
{
    EXPECT_EQ(FileUtil::getSize("/nonexistent"), -1);
}

TEST(FileUtilTest, GetSize_EmptyFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("empty.txt");
    FileUtil::createFile(filePath);

    EXPECT_EQ(FileUtil::getSize(filePath), 0);
}

// ==================== exists / isFile / isDirectory ====================

TEST(FileUtilTest, Exists_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("exists_test.txt");
    EXPECT_FALSE(FileUtil::exists(filePath));

    FileUtil::createFile(filePath);
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, IsFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("is_file_test.txt");
    FileUtil::createFile(filePath);

    EXPECT_TRUE(FileUtil::isFile(filePath));
    EXPECT_FALSE(FileUtil::isDirectory(filePath));
}

TEST(FileUtilTest, IsDirectory)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    EXPECT_TRUE(FileUtil::isDirectory(tmp.path()));
    EXPECT_FALSE(FileUtil::isFile(tmp.path()));
}

// ==================== listFiles / listEntries ====================

TEST(FileUtilTest, ListFiles_Flat)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.createFile("a.txt");
    tmp.createFile("b.txt");

    auto result = FileUtil::listFiles(tmp.path(), false);
    ASSERT_TRUE(result.is_ok()) << "listFiles failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap().size(), 2u);
}

TEST(FileUtilTest, ListFiles_Recursive)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.createFile("a.txt");
    tmp.createFile("sub/b.txt");

    auto result = FileUtil::listFiles(tmp.path(), true);
    ASSERT_TRUE(result.is_ok()) << "listFiles recursive failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap().size(), 2u);
}

TEST(FileUtilTest, ListFiles_DirectoryNotFound)
{
    auto result = FileUtil::listFiles("/nonexistent_dir");
    EXPECT_TRUE(result.is_err());
}

TEST(FileUtilTest, ListEntries)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    tmp.createFile("f1.txt");
    tmp.createFile("f2.txt");
    EXPECT_TRUE(FileUtil::createDirectories(tmp.makePath("subdir")));

    auto result = FileUtil::listEntries(tmp.path());
    ASSERT_TRUE(result.is_ok()) << "listEntries failed: " << result.unwrap_err();
    EXPECT_EQ(result.unwrap().size(), 3u);
}

// ==================== copy ====================

TEST(FileUtilTest, Copy_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.makePath("src.txt");
    auto dst = tmp.makePath("dst.txt");
    EXPECT_TRUE(FileUtil::writeText(src, "copy test"));
    EXPECT_TRUE(FileUtil::copy(src, dst));
    EXPECT_TRUE(FileUtil::exists(dst));

    auto content = FileUtil::readAllText(dst);
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

    auto srcDir = tmp.makePath("srcdir");
    auto dstDir = tmp.makePath("dstdir");
    EXPECT_TRUE(FileUtil::createDirectories(srcDir));
    EXPECT_TRUE(FileUtil::writeText(tmp.makePath("srcdir/a.txt"), "file in dir"));

    EXPECT_TRUE(FileUtil::copy(srcDir, dstDir));
    EXPECT_TRUE(FileUtil::isDirectory(dstDir));
}

// ==================== move ====================

TEST(FileUtilTest, Move_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.makePath("move_src.txt");
    auto dst = tmp.makePath("move_dst.txt");
    EXPECT_TRUE(FileUtil::writeText(src, "move test"));
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

    auto filePath = tmp.makePath("to_remove.txt");
    FileUtil::createFile(filePath);
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

    auto dirPath = tmp.makePath("remove_subdir");
    EXPECT_TRUE(FileUtil::createDirectories(dirPath));
    tmp.createFile("remove_subdir/a.txt");

    EXPECT_TRUE(FileUtil::removeAll(dirPath));
    EXPECT_FALSE(FileUtil::exists(dirPath));
}

// ==================== createFile / createDirectories ====================

TEST(FileUtilTest, CreateFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("brand_new.txt");
    EXPECT_TRUE(FileUtil::createFile(filePath));
    EXPECT_TRUE(FileUtil::exists(filePath));
    EXPECT_TRUE(FileUtil::isFile(filePath));
    EXPECT_EQ(FileUtil::getSize(filePath), 0);
}

TEST(FileUtilTest, CreateFile_WithParentDirectories)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("a/b/c/deep.txt");
    EXPECT_TRUE(FileUtil::createFile(filePath));
    EXPECT_TRUE(FileUtil::exists(filePath));
}

TEST(FileUtilTest, CreateDirectories)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto dirPath = tmp.makePath("deeply/nested/dir/structure");
    EXPECT_TRUE(FileUtil::createDirectories(dirPath));
    EXPECT_TRUE(FileUtil::isDirectory(dirPath));
}

// ==================== backup ====================

TEST(FileUtilTest, Backup_File)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto src = tmp.makePath("backup_me.txt");
    EXPECT_TRUE(FileUtil::writeText(src, "important data"));

    auto result = FileUtil::backup(src);
    ASSERT_TRUE(result.is_ok()) << "backup failed: " << result.unwrap_err();

    EXPECT_TRUE(FileUtil::exists(result.unwrap()));
    EXPECT_NE(result.unwrap(), src);

    auto content = FileUtil::readAllText(result.unwrap());
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
    auto result = FileUtil::createTempFile("libca_", ".tmp");
    ASSERT_TRUE(result.is_ok()) << "createTempFile failed: " << result.unwrap_err();
    EXPECT_TRUE(FileUtil::exists(result.unwrap()));
    EXPECT_TRUE(FileUtil::isFile(result.unwrap()));
    FileUtil::remove(result.unwrap());
}

TEST(FileUtilTest, CreateTempDirectory)
{
    auto result = FileUtil::createTempDirectory("libca_dir_");
    ASSERT_TRUE(result.is_ok()) << "createTempDirectory failed: " << result.unwrap_err();
    EXPECT_TRUE(FileUtil::isDirectory(result.unwrap()));
    FileUtil::removeAll(result.unwrap());
}

// ==================== isReadable / isWritable ====================

TEST(FileUtilTest, IsReadable_ExistingFile)
{
    TempDirGuard tmp;
    ASSERT_TRUE(tmp.valid());

    auto filePath = tmp.makePath("readable.txt");
    FileUtil::createFile(filePath);

    EXPECT_TRUE(FileUtil::isReadable(filePath));
    EXPECT_TRUE(FileUtil::isWritable(filePath));
}

TEST(FileUtilTest, IsReadable_NonExistent)
{
    EXPECT_FALSE(FileUtil::isReadable("/nonexistent/path"));
    EXPECT_FALSE(FileUtil::isWritable("/nonexistent/path"));
}

}}}  // namespace ca::fs::test
