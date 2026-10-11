#include <gmock/gmock.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "libca/fs/file_util.hpp"
#include "libca/fs/stdio_file.hpp"

namespace ca { namespace fs { namespace test {

using namespace testing;

// 测试辅助：临时目录 RAII 守卫（与 file_util_test 同款语义）
class StdioTempDirGuard
{
public:
    StdioTempDirGuard()
    {
        auto result = FileUtil::create_temp_directory("libca_fs_stdio_test_");
        dir_ = result.is_ok() ? std::move(result).unwrap() : std::string();
    }
    ~StdioTempDirGuard()
    {
        if (!dir_.empty())
            FileUtil::remove_all(dir_);
    }
    const std::string& path() const { return dir_; }

private:
    std::string dir_;
};

std::string join_path(const std::string& dir, const std::string& name)
{
    return dir + "/" + name;
}

TEST(StdioFileTest, OpenReadMissingFileMapsToFileNotFound)
{
    StdioTempDirGuard guard;
    const auto opened = open_stdio(Path::from_utf8_lossy(join_path(guard.path(), "no_such.bin")), "rb");
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err(), FsError::FileNotFound);
}

TEST(StdioFileTest, OpenWriteCreateReadRoundTrip)
{
    StdioTempDirGuard guard;
    const Path p = Path::from_utf8_lossy(join_path(guard.path(), "hello.txt"));

    {
        auto opened = open_stdio(p, "wb");
        ASSERT_TRUE(opened.is_ok());
        StdioFile file = std::move(opened).unwrap();
        ASSERT_TRUE(file.is_open());
        const char* text = "hello stdio";
        EXPECT_EQ(std::fwrite(text, 1, std::strlen(text), file.get()), std::strlen(text));
    }  // 析构 fclose

    {
        auto opened = open_stdio(p, "rb");
        ASSERT_TRUE(opened.is_ok());
        StdioFile file = std::move(opened).unwrap();
        std::string buf(11, '\0');
        EXPECT_EQ(std::fread(&buf[0], 1, buf.size(), file.get()), buf.size());
        EXPECT_EQ(buf, "hello stdio");
    }
}

TEST(StdioFileTest, MoveSemanticsTransferOwnership)
{
    StdioTempDirGuard guard;
    const Path p = Path::from_utf8_lossy(join_path(guard.path(), "move.txt"));
    ASSERT_TRUE(FileUtil::write_text(p, "abc").is_ok());

    auto opened = open_stdio(p, "rb");
    ASSERT_TRUE(opened.is_ok());
    StdioFile first = std::move(opened).unwrap();

    StdioFile second(std::move(first));
    EXPECT_FALSE(first.is_open());
    ASSERT_TRUE(second.is_open());

    StdioFile third;
    third = std::move(second);
    EXPECT_FALSE(second.is_open());
    ASSERT_TRUE(third.is_open());

    // release 后对象不再持有句柄，由调用方关闭
    std::FILE* raw = third.release();
    ASSERT_NE(raw, nullptr);
    EXPECT_FALSE(third.is_open());
    EXPECT_EQ(std::fclose(raw), 0);
}

TEST(StdioFileTest, ReadFileAllRoundTripsBinaryLargerThanBuffer)
{
    StdioTempDirGuard guard;
    // > BUFSIZ（通常 4KB）的二进制内容，覆盖"seek 取尺寸 + 单次 fread"与缓冲边界
    std::string payload(64 * 1024, '\0');
    for (std::size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>((i * 7 + 3) & 0xFF);

    const Path p = Path::from_utf8_lossy(join_path(guard.path(), "blob.bin"));
    ASSERT_TRUE(FileUtil::write_text(p, payload).is_ok());

    auto content = read_file_all(p);
    ASSERT_TRUE(content.is_ok());
    EXPECT_EQ(std::move(content).unwrap(), payload);
}

TEST(StdioFileTest, ReadFileAllEmptyFile)
{
    StdioTempDirGuard guard;
    const Path p = Path::from_utf8_lossy(join_path(guard.path(), "empty.txt"));
    ASSERT_TRUE(FileUtil::write_text(p, "").is_ok());

    auto content = read_file_all(p);
    ASSERT_TRUE(content.is_ok());
    EXPECT_TRUE(std::move(content).unwrap().empty());
}

TEST(StdioFileTest, ReadFileAllMissingFileMapsToFileNotFound)
{
    StdioTempDirGuard guard;
    auto content = read_file_all(Path::from_utf8_lossy(join_path(guard.path(), "gone.txt")));
    ASSERT_TRUE(content.is_err());
    EXPECT_EQ(content.unwrap_err(), FsError::FileNotFound);
}

TEST(StdioFileTest, ReadFileAllRejectsDirectory)
{
    StdioTempDirGuard guard;
    // 目录不是可读文件：POSIX 打开成功、fread 报 EISDIR→IsADirectory；
    // Windows 打开/读取失败按 errno 归类——两平台都必须是错误，不断言具体码。
    auto content = read_file_all(Path::from_utf8_lossy(guard.path()));
    EXPECT_TRUE(content.is_err());
}

TEST(StdioFileTest, ReadFileAllNonAsciiUtf8PathRoundTrips)
{
    // #249 关键回归：Windows 窄 fopen 按 ANSI 代码页解释路径，中文/emoji 路径
    // 必然打开失败；open_stdio 须走宽字符 _wfopen。写侧用 FileUtil（ofstream
    // 经 fs::path 宽链路）保证文件确实创建成功，读侧走被测的 stdio 路径。
    StdioTempDirGuard guard;
    const std::string name = "目录-中文-📦-测试.txt";
    const std::string file_path = join_path(guard.path(), name);

    const std::string text = "非 ASCII 路径内容 content-emoji: 📦";
    ASSERT_TRUE(FileUtil::write_text(file_path, text).is_ok());

    auto content = read_file_all(Path::from_utf8_lossy(file_path));
    ASSERT_TRUE(content.is_ok());
    EXPECT_EQ(std::move(content).unwrap(), text);
}

}}}  // namespace ca::fs::test
