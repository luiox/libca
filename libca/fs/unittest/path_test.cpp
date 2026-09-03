#include <gmock/gmock.h>

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>

#include "libca/fs/path.hpp"

namespace ca { namespace fs { namespace test {

using namespace testing;

namespace {

// 拆串构造非法 UTF-8 字面量：\xff 后若直接跟 hex 字母（如 b）会被解析成多字节十六进制转义。
// 单个非法字节在两平台都恰好替换为一个 U+FFFD，断言跨平台确定。
const char* k_invalid_utf8 = "a\xff" "b";
const char* k_replacement_suffix = "a\xef\xbf\xbd" "b";  // U+FFFD = EF BF BD

Path utf8_path(std::string_view s)
{
    auto r = Path::from_utf8(s);
    EXPECT_TRUE(r.is_ok());
    return r.unwrap();
}

}  // namespace

// ==================== from_utf8 / from_utf8_lossy ====================

TEST(PathTest, FromUtf8_ValidUtf8_Ok)
{
    auto r = Path::from_utf8(u8"中文/目录/文件.txt");
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.unwrap().to_utf8_lossy(), u8"中文/目录/文件.txt");
}

TEST(PathTest, FromUtf8_Ascii_Ok)
{
    auto r = Path::from_utf8("a/b/c.txt");
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.unwrap().to_utf8_lossy(), "a/b/c.txt");
}

TEST(PathTest, FromUtf8_Empty_Ok)
{
    auto r = Path::from_utf8("");
    ASSERT_TRUE(r.is_ok());
    EXPECT_TRUE(r.unwrap().is_empty());
}

TEST(PathTest, FromUtf8_InvalidBytes_ReturnsInvalidUtf8Error)
{
    auto r = Path::from_utf8(k_invalid_utf8);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), FsError::InvalidUtf8);
    EXPECT_EQ(to_string(r.unwrap_err()), "path is not valid UTF-8");
}

TEST(PathTest, FromUtf8_TruncatedSequence_ReturnsError)
{
    // 0xE4 0xB8 需要 3 字节，这里截断为 2 字节
    auto r = Path::from_utf8("a\xe4\xb8");
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), FsError::InvalidUtf8);
}

TEST(PathTest, FromUtf8Lossy_Valid_Unchanged)
{
    auto p = Path::from_utf8_lossy(u8"中文/文件.txt");
    EXPECT_EQ(p.to_utf8_lossy(), u8"中文/文件.txt");
}

TEST(PathTest, FromUtf8Lossy_InvalidBytes_ReplacedWithU8FFFD)
{
    // 两平台统一契约：非法序列替换为 U+FFFD，绝不失败
    auto p = Path::from_utf8_lossy(k_invalid_utf8);
    EXPECT_EQ(p.to_utf8_lossy(), k_replacement_suffix);
}

TEST(PathTest, FromUtf8Lossy_Empty_EmptyPath)
{
    EXPECT_TRUE(Path::from_utf8_lossy("").is_empty());
}

// ==================== OsString / native 互操作 ====================

TEST(PathTest, OsStringRoundTrip)
{
    auto p = utf8_path(u8"目录/中文.txt");
    auto back = Path::from_os_string(p.to_os_string());
    EXPECT_EQ(p, back);
}

TEST(PathTest, NativeInterop)
{
    auto p = utf8_path("a/b");
    EXPECT_EQ(p.native(), std::filesystem::path("a/b"));

    auto taken = Path(std::filesystem::path("x/y"));
    EXPECT_EQ(taken.to_utf8_lossy(), "x/y");
}

#ifdef _WIN32
TEST(PathTest, FromNative_UnpairedSurrogate_LossyExportReplaces)
{
    // NTFS 允许未配对代理的文件名：from_native 无损承载，lossy 导出按 U+FFFD 替代。
    // to_utf8_lossy 为 generic 格式，反斜杠分隔符输出为 '/'。
    const wchar_t wide_name[] = L"a\\\xd800" L"b";
    auto p = Path::from_native(wide_name);
    EXPECT_EQ(p.to_utf8_lossy(), "a/\xef\xbf\xbd" "b");
}
#endif

// ==================== 分隔符与格式 ====================

#ifdef _WIN32
TEST(PathTest, ToUtf8Lossy_NormalizesSeparatorsToForwardSlash)
{
    EXPECT_EQ(Path::from_utf8_lossy("a\\b\\c").to_utf8_lossy(), "a/b/c");
    EXPECT_EQ(Path::from_utf8_lossy("C:\\Users\\t").to_utf8_lossy(), "C:/Users/t");
}
#else
TEST(PathTest, ToUtf8Lossy_KeepsBackslashAsFilenameChar)
{
    // POSIX 上反斜杠是文件名字符，不是分隔符
    EXPECT_EQ(Path::from_utf8_lossy("a\\b").to_utf8_lossy(), "a\\b");
}
#endif

// ==================== 组合 / 分解 ====================

TEST(PathTest, ParentFilenameStemExtension)
{
    auto p = utf8_path("a/b/c.txt");
    EXPECT_EQ(p.parent().to_utf8_lossy(), "a/b");
    EXPECT_EQ(p.filename().to_utf8_lossy(), "c.txt");
    EXPECT_EQ(p.stem().to_utf8_lossy(), "c");
    EXPECT_EQ(p.extension(), ".txt");
}

TEST(PathTest, Filename_WithChinese)
{
    auto p = utf8_path(u8"目录/中文文件.txt");
    EXPECT_EQ(p.filename().to_utf8_lossy(), u8"中文文件.txt");
    EXPECT_EQ(p.stem().to_utf8_lossy(), u8"中文文件");
    EXPECT_EQ(p.extension(), ".txt");
}

TEST(PathTest, Parent_Root_IsItself)
{
    auto root = utf8_path("/");
    EXPECT_EQ(root.parent(), root);
    EXPECT_EQ(root.filename().to_utf8_lossy(), "");
}

TEST(PathTest, Parent_TrailingSeparator)
{
    auto p = utf8_path("a/b/");
    EXPECT_EQ(p.parent().to_utf8_lossy(), "a/b");
    EXPECT_EQ(p.filename().to_utf8_lossy(), "");
}

TEST(PathTest, Extension_NoExtension_Empty)
{
    EXPECT_EQ(utf8_path("a/b").extension(), "");
    EXPECT_EQ(utf8_path("a/b.").extension(), ".");
}

TEST(PathTest, EmptyPath_DecompositionEmpty)
{
    Path p;
    EXPECT_TRUE(p.is_empty());
    EXPECT_EQ(p.to_utf8_lossy(), "");
    EXPECT_TRUE(p.parent().is_empty());
    EXPECT_TRUE(p.filename().is_empty());
}

TEST(PathTest, OperatorSlash_Joins)
{
    auto joined = utf8_path("a") / utf8_path("b") / utf8_path("c.txt");
    EXPECT_EQ(joined.to_utf8_lossy(), "a/b/c.txt");
}

TEST(PathTest, OperatorSlash_AbsoluteTailReplaces)
{
    // std::filesystem 语义：右操作数带根目录时整体替换
    auto joined = utf8_path("a/b") / utf8_path("/c");
    EXPECT_EQ(joined.to_utf8_lossy(), "/c");
}

TEST(PathTest, OperatorSlashAssign_Mutates)
{
    auto p = utf8_path("a");
    p /= utf8_path("b");
    EXPECT_EQ(p.to_utf8_lossy(), "a/b");
}

TEST(PathTest, Normalized_RemovesDotAndDotDot)
{
    EXPECT_EQ(utf8_path("a/./b/../c").normalized().to_utf8_lossy(), "a/c");
}

TEST(PathTest, IsAbsolute)
{
#ifdef _WIN32
    EXPECT_TRUE(utf8_path("C:/x").is_absolute());
    EXPECT_FALSE(utf8_path("/x").is_absolute());  // 无盘符：有根目录但非绝对
#else
    EXPECT_TRUE(utf8_path("/x").is_absolute());
#endif
    EXPECT_FALSE(utf8_path("a/b").is_absolute());
    EXPECT_FALSE(Path().is_absolute());
}

TEST(PathTest, Absolute_RelativeBecomesAbsolute)
{
    auto r = utf8_path("a/b.txt").absolute();
    ASSERT_TRUE(r.is_ok());
    EXPECT_TRUE(r.unwrap().is_absolute());
}

TEST(PathTest, Components_Split)
{
    auto parts = utf8_path("a/b/c").components();
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0].to_utf8_lossy(), "a");
    EXPECT_EQ(parts[1].to_utf8_lossy(), "b");
    EXPECT_EQ(parts[2].to_utf8_lossy(), "c");
}

#ifdef _WIN32
TEST(PathTest, Components_DriveLetterIsOwnComponent)
{
    // MSVC 迭代 "C:/a/b" 得 ["C:", "/", "a", "b"]：盘符与根目录均为独立组件；
    // 断言按结构（首为盘符、尾为文件名），不钉死实现相关的根目录元素。
    auto parts = utf8_path("C:/a/b").components();
    ASSERT_GE(parts.size(), 3u);
    EXPECT_EQ(parts.front().to_utf8_lossy(), "C:");
    EXPECT_EQ(parts.back().to_utf8_lossy(), "b");
    EXPECT_EQ(parts[parts.size() - 2].to_utf8_lossy(), "a");
}
#endif

TEST(PathTest, Components_RootIsSingleComponent)
{
    auto parts = utf8_path("/").components();
    ASSERT_EQ(parts.size(), 1u);
}

// ==================== 比较 / 哈希 ====================

TEST(PathTest, Equality)
{
    EXPECT_EQ(utf8_path("a/b"), utf8_path("a/b"));
    EXPECT_NE(utf8_path("a/b"), utf8_path("a/c"));
}

#ifdef _WIN32
TEST(PathTest, Equality_FoldsSeparatorsOnWindows)
{
    // Windows 上 '/' 与 '\\' 是同一分隔符，native 词法比较视为相等
    EXPECT_EQ(utf8_path("a\\b"), utf8_path("a/b"));
}
#endif

TEST(PathTest, Less_OrderingUsableAsMapKey)
{
    std::map<Path, int> m;
    m[utf8_path("a/c")] = 2;
    m[utf8_path("a/b")] = 1;
    ASSERT_EQ(m.size(), 2u);
    EXPECT_EQ(m.begin()->first, utf8_path("a/b"));  // 词法序在前者排头
}

TEST(PathTest, Hash_UsableInUnorderedMap)
{
    std::unordered_map<Path, int> m;
    m[utf8_path("a/b")] = 1;
    m[utf8_path("c/d")] = 2;
    EXPECT_EQ(m[utf8_path("a/b")], 1);
    EXPECT_EQ(m[utf8_path("c/d")], 2);
    EXPECT_EQ(m[utf8_path("a/b")], m[utf8_path("a/b")]);
}

TEST(PathTest, Hash_EqualPathsHaveEqualHash)
{
    auto h = std::hash<Path>{};
    EXPECT_EQ(h(utf8_path("x/y")), h(utf8_path("x/y")));
}

}}}  // namespace ca::fs::test
