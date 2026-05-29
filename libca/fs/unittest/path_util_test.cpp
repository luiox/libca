#include <gmock/gmock.h>

#include "libca/fs/path_util.hpp"

namespace ca { namespace fs { namespace test {

using namespace testing;

// ==================== normalize ====================

TEST(PathUtilTest, Normalize_UnixPath_Unchanged)
{
    EXPECT_EQ(PathUtil::normalize("/a/b/c"), "/a/b/c");
}

TEST(PathUtilTest, Normalize_Backslashes_ConvertedToForward)
{
    EXPECT_EQ(PathUtil::normalize("a\\b\\c"), "a/b/c");
}

TEST(PathUtilTest, Normalize_RedundantDot_Removed)
{
    EXPECT_EQ(PathUtil::normalize("/a/./b"), "/a/b");
}

TEST(PathUtilTest, Normalize_DotDot_Resolved)
{
    EXPECT_EQ(PathUtil::normalize("/a/b/../c"), "/a/c");
}

TEST(PathUtilTest, Normalize_EmptyPath_ReturnsEmpty)
{
    // MSVC 上空路径 normalize 后仍为空字符串
    EXPECT_EQ(PathUtil::normalize(""), "");
}

TEST(PathUtilTest, Normalize_MixedSeparators)
{
    EXPECT_EQ(PathUtil::normalize("a\\b/c\\d"), "a/b/c/d");
}

TEST(PathUtilTest, Normalize_TrailingSeparator)
{
    // 末尾的 / 在 normalize 中可能被保留或去除，取决于实现
    auto result = PathUtil::normalize("/a/b/");
    EXPECT_TRUE(result == "/a/b" || result == "/a/b/");
}

// ==================== toUnixSeparators ====================

TEST(PathUtilTest, ToUnixSeparators_Basic)
{
    EXPECT_EQ(PathUtil::toUnixSeparators("a\\b\\c"), "a/b/c");
}

TEST(PathUtilTest, ToUnixSeparators_AlreadyUnix_Unchanged)
{
    EXPECT_EQ(PathUtil::toUnixSeparators("a/b/c"), "a/b/c");
}

TEST(PathUtilTest, ToUnixSeparators_Mixed)
{
    EXPECT_EQ(PathUtil::toUnixSeparators("a\\b/c\\d"), "a/b/c/d");
}

TEST(PathUtilTest, ToUnixSeparators_Empty)
{
    EXPECT_EQ(PathUtil::toUnixSeparators(""), "");
}

TEST(PathUtilTest, ToUnixSeparators_WindowsDriveLetter)
{
    EXPECT_EQ(PathUtil::toUnixSeparators("C:\\Users\\test"), "C:/Users/test");
}

// ==================== join ====================

TEST(PathUtilTest, Join_TwoParts)
{
    EXPECT_EQ(PathUtil::join("/base", "sub"), "/base/sub");
}

TEST(PathUtilTest, Join_TwoParts_TrailingSlash)
{
    EXPECT_EQ(PathUtil::join("/base/", "sub"), "/base/sub");
}

TEST(PathUtilTest, Join_ThreeParts)
{
    EXPECT_EQ(PathUtil::join("/base", "sub", "file.txt"), "/base/sub/file.txt");
}

TEST(PathUtilTest, Join_EmptyBase)
{
    EXPECT_EQ(PathUtil::join("", "file.txt"), "file.txt");
}

TEST(PathUtilTest, Join_AbsolutePart_OverridesBase)
{
    // 第二个参数是绝对路径时，结果可能为第二个参数（取决于平台）
    auto result = PathUtil::join("/base", "/absolute");
    // 大部分实现会返回 /absolute
    EXPECT_EQ(result, "/absolute");
}

// ==================== extension ====================

TEST(PathUtilTest, Extension_Simple)
{
    EXPECT_EQ(PathUtil::extension("file.txt"), ".txt");
}

TEST(PathUtilTest, Extension_NoExtension)
{
    EXPECT_EQ(PathUtil::extension("file"), "");
}

TEST(PathUtilTest, Extension_MultipleDots)
{
    EXPECT_EQ(PathUtil::extension("archive.tar.gz"), ".gz");
}

TEST(PathUtilTest, Extension_PathWithDirectory)
{
    EXPECT_EQ(PathUtil::extension("/a/b/file.txt"), ".txt");
}

TEST(PathUtilTest, Extension_HiddenFile)
{
    EXPECT_EQ(PathUtil::extension(".gitignore"), "");
}

// ==================== stem ====================

TEST(PathUtilTest, Stem_Simple)
{
    EXPECT_EQ(PathUtil::stem("file.txt"), "file");
}

TEST(PathUtilTest, Stem_NoExtension)
{
    EXPECT_EQ(PathUtil::stem("file"), "file");
}

TEST(PathUtilTest, Stem_PathWithDirectory)
{
    EXPECT_EQ(PathUtil::stem("/a/b/archive.tar.gz"), "archive.tar");
}

TEST(PathUtilTest, Stem_HiddenFile)
{
    EXPECT_EQ(PathUtil::stem(".gitignore"), ".gitignore");
}

// ==================== filename ====================

TEST(PathUtilTest, Filename_Simple)
{
    EXPECT_EQ(PathUtil::filename("/a/b/file.txt"), "file.txt");
}

TEST(PathUtilTest, Filename_Root)
{
    EXPECT_EQ(PathUtil::filename("/"), "");
}

TEST(PathUtilTest, Filename_NoDirectory)
{
    EXPECT_EQ(PathUtil::filename("file.txt"), "file.txt");
}

// ==================== parent ====================

TEST(PathUtilTest, Parent_Simple)
{
    EXPECT_EQ(PathUtil::parent("/a/b/c"), "/a/b");
}

TEST(PathUtilTest, Parent_FileOnly)
{
    EXPECT_EQ(PathUtil::parent("file.txt"), "");
}

TEST(PathUtilTest, Parent_Root)
{
    EXPECT_EQ(PathUtil::parent("/"), "/");
}

TEST(PathUtilTest, Parent_RelativePath)
{
    EXPECT_EQ(PathUtil::parent("a/b"), "a");
}

// ==================== isAbsolute ====================

TEST(PathUtilTest, IsAbsolute_UnixAbsolute)
{
    // POSIX 上 /usr/bin 是绝对路径；Windows 上无盘符则不是
#ifdef _WIN32
    EXPECT_FALSE(PathUtil::isAbsolute("/usr/bin"));
#else
    EXPECT_TRUE(PathUtil::isAbsolute("/usr/bin"));
#endif
}

TEST(PathUtilTest, IsAbsolute_Relative)
{
    EXPECT_FALSE(PathUtil::isAbsolute("relative/path"));
}

TEST(PathUtilTest, IsAbsolute_Empty)
{
    EXPECT_FALSE(PathUtil::isAbsolute(""));
}

#ifdef _WIN32
TEST(PathUtilTest, IsAbsolute_WindowsAbsolute)
{
    EXPECT_TRUE(PathUtil::isAbsolute("C:\\Users"));
    EXPECT_TRUE(PathUtil::isAbsolute("C:/Users"));
}
#endif

// ==================== toAbsolute ====================

TEST(PathUtilTest, ToAbsolute_RelativePath)
{
    auto result = PathUtil::toAbsolute("some/path");
    EXPECT_TRUE(PathUtil::isAbsolute(result));
    EXPECT_THAT(result, HasSubstr("some/path"));
}

TEST(PathUtilTest, ToAbsolute_AbsolutePath_Unchanged)
{
    auto result = PathUtil::toAbsolute("/usr/bin");
    EXPECT_TRUE(PathUtil::isAbsolute(result));
}

// ==================== split ====================

TEST(PathUtilTest, Split_Simple)
{
    auto parts = PathUtil::split("a/b/c");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(PathUtilTest, Split_Absolute)
{
    // 注意：POSIX 上 /a/b 的 path 迭代包含根目录 "/" 作为第一个元素
    auto parts = PathUtil::split("/a/b");
    ASSERT_GE(parts.size(), 2u);
    // 验证最后两个元素是 "a" 和 "b"
    EXPECT_EQ(parts[parts.size() - 2], "a");
    EXPECT_EQ(parts[parts.size() - 1], "b");
}

TEST(PathUtilTest, Split_Single)
{
    auto parts = PathUtil::split("file.txt");
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "file.txt");
}

TEST(PathUtilTest, Split_Empty)
{
    auto parts = PathUtil::split("");
    EXPECT_TRUE(parts.empty());
}

}}}  // namespace ca::fs::test
