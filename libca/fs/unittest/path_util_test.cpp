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

// ==================== normalizeWithin ====================

TEST(PathUtilTest, NormalizeWithin_SubPathNormalized)
{
    // sub/.. 相互抵消，结果折回 base 直下
    auto result = PathUtil::normalize_within("base/sub/../ok.txt", "base");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap(), "base/ok.txt");
}

TEST(PathUtilTest, NormalizeWithin_DotSegmentsCollapsed)
{
    auto result = PathUtil::normalize_within("base/./sub/./x.txt", "base");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap(), "base/sub/x.txt");
}

TEST(PathUtilTest, NormalizeWithin_TraversalRejected)
{
    auto result = PathUtil::normalize_within("base/../../escape.txt", "base");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err(), FsError::PathOutsideBase);
}

TEST(PathUtilTest, NormalizeWithin_DeepTraversalRejected)
{
    // sub/.. 折叠后仍向上逃逸出 base
    auto result = PathUtil::normalize_within("/base/sub/../../escape.txt", "/base");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err(), FsError::PathOutsideBase);
}

TEST(PathUtilTest, NormalizeWithin_AbsolutePathInjectionRejected)
{
    auto result = PathUtil::normalize_within("/etc/passwd", "/var/www");
    EXPECT_TRUE(result.is_err());
}

TEST(PathUtilTest, NormalizeWithin_AbsoluteIntoRelativeBaseRejected)
{
    auto result = PathUtil::normalize_within("/tmp/evil", "base");
    EXPECT_TRUE(result.is_err());
}

TEST(PathUtilTest, NormalizeWithin_RelativeIntoAbsoluteBaseRejected)
{
    auto result = PathUtil::normalize_within("evil.txt", "/var/www");
    EXPECT_TRUE(result.is_err());
}

TEST(PathUtilTest, NormalizeWithin_BaseItselfAllowed)
{
    // 设计决定：path 归一化后等于 base 视为在 base 之内（含边界），返回规范化后的 base。
    auto result = PathUtil::normalize_within("base", "base");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap(), "base");

    auto boundary = PathUtil::normalize_within("/var/www", "/var/www");
    ASSERT_TRUE(boundary.is_ok());
    EXPECT_EQ(std::move(boundary).unwrap(), "/var/www");
}

TEST(PathUtilTest, NormalizeWithin_SubDotDotFoldsToBase)
{
    // 词法归一化语义（lexically_normal）在 .. 被消费后保留 base 的尾分隔符
    auto result = PathUtil::normalize_within("base/sub/..", "base");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap(), "base/");
}

TEST(PathUtilTest, NormalizeWithin_EmptyPathRejected)
{
    EXPECT_TRUE(PathUtil::normalize_within("", "base").is_err());
}

TEST(PathUtilTest, NormalizeWithin_EmptyBaseRejected)
{
    EXPECT_TRUE(PathUtil::normalize_within("base", "").is_err());
    EXPECT_TRUE(PathUtil::normalize_within("", "").is_err());
}

TEST(PathUtilTest, NormalizeWithin_BackslashesFolded)
{
    // 反斜杠先统一为 '/'，再词法归一化（sub/.. 相互抵消）
    auto result = PathUtil::normalize_within("base\\sub\\..\\file.txt", "base");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap(), "base/file.txt");
}

#ifdef _WIN32
TEST(PathUtilTest, NormalizeWithin_WindowsDriveLetterCaseInsensitive)
{
    // 盘符大小写不参与逃逸判定：c:/ 与 C:/ 视为同一根；结果保留 path 原书写形式。
    auto result = PathUtil::normalize_within("c:/data/sub/file.txt", "C:/data");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(std::move(result).unwrap(), "c:/data/sub/file.txt");
}

TEST(PathUtilTest, NormalizeWithin_WindowsCaseSensitiveSegmentsConservative)
{
    // 目录段大小写按词法精确比较：对大小写不敏感文件系统宁可误拒、不可误放。
    auto result = PathUtil::normalize_within("C:/data/../DATA/secret.txt", "C:/data");
    EXPECT_TRUE(result.is_err());
}

TEST(PathUtilTest, NormalizeWithin_WindowsDifferentDriveRejected)
{
    auto result = PathUtil::normalize_within("D:/somewhere/file.txt", "C:/data");
    EXPECT_TRUE(result.is_err());
}

TEST(PathUtilTest, NormalizeWithin_WindowsBackslashTraversalRejected)
{
    auto result = PathUtil::normalize_within("C:\\data\\..\\..\\windows\\system32", "C:\\data");
    EXPECT_TRUE(result.is_err());
}
#endif

// ==================== toUnixSeparators ====================

TEST(PathUtilTest, ToUnixSeparators_Basic)
{
    EXPECT_EQ(PathUtil::to_unix_separators("a\\b\\c"), "a/b/c");
}

TEST(PathUtilTest, ToUnixSeparators_AlreadyUnix_Unchanged)
{
    EXPECT_EQ(PathUtil::to_unix_separators("a/b/c"), "a/b/c");
}

TEST(PathUtilTest, ToUnixSeparators_Mixed)
{
    EXPECT_EQ(PathUtil::to_unix_separators("a\\b/c\\d"), "a/b/c/d");
}

TEST(PathUtilTest, ToUnixSeparators_Empty)
{
    EXPECT_EQ(PathUtil::to_unix_separators(""), "");
}

TEST(PathUtilTest, ToUnixSeparators_WindowsDriveLetter)
{
    EXPECT_EQ(PathUtil::to_unix_separators("C:\\Users\\test"), "C:/Users/test");
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
    EXPECT_FALSE(PathUtil::is_absolute("/usr/bin"));
#else
    EXPECT_TRUE(PathUtil::is_absolute("/usr/bin"));
#endif
}

TEST(PathUtilTest, IsAbsolute_Relative)
{
    EXPECT_FALSE(PathUtil::is_absolute("relative/path"));
}

TEST(PathUtilTest, IsAbsolute_Empty)
{
    EXPECT_FALSE(PathUtil::is_absolute(""));
}

#ifdef _WIN32
TEST(PathUtilTest, IsAbsolute_WindowsAbsolute)
{
    EXPECT_TRUE(PathUtil::is_absolute("C:\\Users"));
    EXPECT_TRUE(PathUtil::is_absolute("C:/Users"));
}
#endif

// ==================== toAbsolute ====================

TEST(PathUtilTest, ToAbsolute_RelativePath)
{
    auto result = PathUtil::to_absolute("some/path");
    EXPECT_TRUE(PathUtil::is_absolute(result));
    EXPECT_THAT(result, HasSubstr("some/path"));
}

TEST(PathUtilTest, ToAbsolute_AbsolutePath_Unchanged)
{
    auto result = PathUtil::to_absolute("/usr/bin");
    EXPECT_TRUE(PathUtil::is_absolute(result));
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

// Windows Unicode 路径回归：中文路径的解析/拼接不应丢字。
TEST(PathUtilTest, UnicodePath_FilenameStemExtension)
{
    EXPECT_EQ(PathUtil::filename(u8"目录/文件.txt"), u8"文件.txt");
    EXPECT_EQ(PathUtil::stem(u8"目录/文件.txt"), u8"文件");
    EXPECT_EQ(PathUtil::extension(u8"目录/文件.txt"), ".txt");
    EXPECT_EQ(PathUtil::parent(u8"目录/文件.txt"), u8"目录");
}

TEST(PathUtilTest, UnicodePath_JoinPreservesCharacters)
{
    auto joined = PathUtil::join(u8"中文目录", u8"子文件.txt");
    EXPECT_EQ(joined, u8"中文目录/子文件.txt");
}

TEST(PathUtilTest, UnicodePath_NormalizePreservesCharacters)
{
    EXPECT_EQ(PathUtil::normalize(u8"目录/./文件.txt"), u8"目录/文件.txt");
}

TEST(PathUtilTest, UnicodePath_SplitPreservesCharacters)
{
    auto parts = PathUtil::split(u8"目录/文件.txt");
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], u8"目录");
    EXPECT_EQ(parts[1], u8"文件.txt");
}

}}}  // namespace ca::fs::test
