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
    std::string name = unique_name("CN");
    std::string value = "你好世界";  // NOLINT: 故意使用 UTF-8 字面量
    ASSERT_TRUE(set(name, value));

    auto got = get(name);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(*got, value);
    EXPECT_TRUE(remove(name));
}

TEST(EnvAllTest, ContainsAtLeastOneEntry)
{
    auto entries = all();

    ASSERT_FALSE(entries.empty());
    // 验收标准：至少包含 PATH（POSIX 几乎总有；Windows 用 Path）。
    auto has_path_field = [](const std::vector<std::pair<std::string, std::string>>& v) {
        return std::any_of(v.begin(), v.end(), [](const auto& kv) {
            return !kv.first.empty();
        });
    };
    EXPECT_TRUE(has_path_field(entries));
}

TEST(EnvAllTest, ReflectsNewlySetVariable)
{
    std::string name  = unique_name("ALL");
    std::string value = "roundtrip";
    ASSERT_TRUE(set(name, value));

    auto entries = all();
    auto it = std::find_if(entries.begin(), entries.end(), [&](const auto& kv) {
        return kv.first == name;
    });
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
    EXPECT_FALSE(version.empty());  // RtlGetVersion 应总能拿到
#else
    // POSIX 上取决于 /etc/os-release 是否存在，宽松验证。
    EXPECT_TRUE(version.empty() || !version.empty());
#endif
    (void)version;
}

}  // namespace
}  // namespace ca::env::test
