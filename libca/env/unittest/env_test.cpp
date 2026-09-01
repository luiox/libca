#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "libca/env/env.hpp"

namespace ca::env::test {
namespace {

// 独占的测试变量前缀，避免污染真实环境；每个用例用唯一名。
std::string unique_name(const char* suffix)
{
    static int counter = 0;
    return "LIBCA_TEST_" + std::to_string(counter++) + "_" + suffix;
}

TEST(EnvGetTest, ReturnsNulloptForMissing)
{
    auto value = get("LIBCA_DEFINITELY_DOES_NOT_EXIST_18265");
    EXPECT_FALSE(value.has_value());
}

TEST(EnvGetTest, ReturnsExistingValue)
{
    std::string name = unique_name("GET");
    ASSERT_TRUE(set(name, "hello"));

    auto value = get(name);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "hello");

    EXPECT_TRUE(remove(name));
}

// 验收标准(1)：get("PATH") 应返回非空值（环境通常都带 PATH/Path）。
TEST(EnvGetTest, ReturnsPathVariable)
{
    // Windows 上变量名是 "Path"（大小写不敏感），POSIX 上是 "PATH"。
    auto posix_path = get("PATH");
    auto win_path   = get("Path");
    ASSERT_TRUE(posix_path.has_value() || win_path.has_value());
    if (posix_path.has_value())
        EXPECT_FALSE(posix_path->empty());
    if (win_path.has_value())
        EXPECT_FALSE(win_path->empty());
}

TEST(EnvSetTest, OverwritesExistingValue)
{
    std::string name = unique_name("SET");
    ASSERT_TRUE(set(name, "first"));
    ASSERT_TRUE(set(name, "second"));

    EXPECT_EQ(*get(name), "second");
    EXPECT_TRUE(remove(name));
}

TEST(EnvRemoveTest, RemovesExistingVariable)
{
    std::string name = unique_name("RM");
    ASSERT_TRUE(set(name, "x"));
    EXPECT_TRUE(remove(name));
    EXPECT_FALSE(get(name).has_value());
}

TEST(EnvRemoveTest, MissingVariableIsNotError)
{
    EXPECT_TRUE(remove("LIBCA_DEFINITELY_DOES_NOT_EXIST_7741"));
}

TEST(EnvChineseRoundtripTest, NonAsciiValuePreserved)
{
    // 验收标准：Windows 上含中文的环境变量正确往返。
    std::string name  = unique_name("CN");
    std::string value = "你好世界";   // NOLINT: 故意使用 UTF-8 字面量
    ASSERT_TRUE(set(name, value));

    auto got = get(name);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(*got, value);
    EXPECT_TRUE(remove(name));
}

TEST(EnvAllTest, ContainsPathVariable)
{
    auto entries = all();

    ASSERT_FALSE(entries.empty());
    // 验收标准(3)：至少包含 PATH（POSIX 上是 "PATH"，Windows 上是 "Path"，
    // Windows 变量名大小写不敏感，all() 返回的 key 大小写取决于系统，故做大小写不敏感匹配。
    auto iequals = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            char ca = a[i], cb = b[i];
            if (ca >= 'A' && ca <= 'Z')
                ca = static_cast<char>(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z')
                cb = static_cast<char>(cb - 'A' + 'a');
            if (ca != cb)
                return false;
        }
        return true;
    };
    bool has_path = std::any_of(
        entries.begin(), entries.end(), [&](const auto& kv) { return iequals(kv.first, "path"); });
    EXPECT_TRUE(has_path);
}

TEST(EnvAllTest, ReflectsNewlySetVariable)
{
    std::string name  = unique_name("ALL");
    std::string value = "roundtrip";
    ASSERT_TRUE(set(name, value));

    auto entries = all();
    auto it      = std::find_if(
        entries.begin(), entries.end(), [&](const auto& kv) { return kv.first == name; });
    ASSERT_NE(it, entries.end());
    EXPECT_EQ(it->second, value);

    EXPECT_TRUE(remove(name));
}

TEST(EnvCurrentDirTest, ReturnsNonEmpty)
{
    std::string cwd = current_dir();
    EXPECT_FALSE(cwd.empty());
}

TEST(EnvSetCurrentDirTest, CanChangeAndRestore)
{
    std::string original = current_dir();
    ASSERT_FALSE(original.empty());

    // 切到临时目录再切回来；路径分隔符/大小写差异下不强求字符串相等，
    // 只要切换成功并能还原即可。
    std::string tmp = temp_dir();
    ASSERT_FALSE(tmp.empty());
    EXPECT_TRUE(set_current_dir(tmp));

    EXPECT_TRUE(set_current_dir(original));
}

TEST(EnvTempDirTest, ReturnsNonEmpty)
{
    std::string tmp = temp_dir();
    EXPECT_FALSE(tmp.empty());
}

// 验收标准：temp_dir() 末尾不应带目录分隔符。把 TMP 设为带尾分隔符的路径，
// 验证剥离逻辑而非环境默认值（Windows 的 GetTempPath 固定以 '\' 结尾，
// 且要求目录有效，故用当前 temp_dir() 作为基底）。
TEST(EnvTempDirTest, NoTrailingSeparator)
{
    std::string base = temp_dir();
    ASSERT_FALSE(base.empty());

#if defined(_WIN32)
    std::string sep = "\\";
#else
    std::string sep = "/";
#endif
    const char* saved = std::getenv("TMP");
    ASSERT_TRUE(set("TMP", base + sep));

    std::string tmp = temp_dir();
    EXPECT_EQ(tmp, base);
    EXPECT_NE(tmp.back(), '/');
#if defined(_WIN32)
    EXPECT_NE(tmp.back(), '\\');
#endif

    // 恢复原环境变量，避免影响后续用例。
    if (saved != nullptr)
        EXPECT_TRUE(set("TMP", saved));
    else
        EXPECT_TRUE(remove("TMP"));
}

TEST(EnvExecutablePathTest, ReturnsNonEmpty)
{
    std::string exe = executable_path();
    EXPECT_FALSE(exe.empty());
}

TEST(EnvOsNameTest, MatchesKnownPlatforms)
{
    std::string name = os_name();
    EXPECT_TRUE(name == "windows" || name == "linux" || name == "macos" || name == "unknown");
}

TEST(EnvOsVersionTest, ReturnsNonEmptyOnWindows)
{
    std::string version = os_version();
#if defined(_WIN32)
    EXPECT_FALSE(version.empty());   // RtlGetVersion 应总能拿到
#else
    // POSIX 上取决于 /etc/os-release 是否存在：可能为空；非空时应是不含换行的版本串。
    if (!version.empty()) {
        EXPECT_EQ(version.find('\n'), std::string::npos);
        EXPECT_GT(version.size(), 0u);
    }
#endif
    (void)version;
}

}   // namespace
}   // namespace ca::env::test
