// libca.test 端到端单测：CWD = libca/test（见 xmake set_rundir），
// fixture 布局：
//   .project_root_file            → "libca_test"（当前子项目）
//   test_resource/sample.txt      → 当前项目资源
//   unittest/fake_proj/.project_root_file → "fake_proj"（跨项目定位）
//   unittest/fake_proj/test_resource/cross.txt
//
// 输出根经环境变量 LIBCA_TEST_OUT_ROOT 指到系统临时目录，
// 不污染仓库工作区。

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <libca/test/test.hpp>

namespace {

namespace fs = std::filesystem;

/// 跨平台设置环境变量（空值表示删除）。
void set_env(const char* name, const char* value)
{
#ifdef _WIN32
    std::string entry = std::string(name) + "=" + value;
    _putenv(entry.c_str());
#else
    if (value && value[0]) {
        setenv(name, value, 1);
    }
    else {
        unsetenv(name);
    }
#endif
}

std::string as_string(const std::vector<uint8_t>& data)
{
    return std::string(data.begin(), data.end());
}

}   // namespace

TEST(TestLibTest, SetupResolvesCurrentAndTop)
{
    ca::test::setup("libca_test");

    // CWD = libca/test：它既是顶层仓根，也是当前子项目根。
    EXPECT_EQ(fs::weakly_canonical(ca::test::current_project_path()),
              fs::weakly_canonical(fs::current_path()));
    EXPECT_EQ(fs::weakly_canonical(ca::test::top_project_path()),
              fs::weakly_canonical(fs::current_path()));
}

TEST(TestLibTest, UnknownProjectNameThrows)
{
    EXPECT_THROW((void)ca::test::setup("no_such_project_xyz"), std::runtime_error);

    // 失败后映射与当前项目保持不变（扫描不被重做，状态不被破坏）。
    ca::test::setup("libca_test");
    EXPECT_EQ(fs::weakly_canonical(ca::test::current_project_path()),
              fs::weakly_canonical(fs::current_path()));
}

TEST(TestLibTest, ResourceLookupAndContent)
{
    ca::test::setup("libca_test");

    EXPECT_TRUE(ca::test::has_resource("sample.txt"));
    EXPECT_FALSE(ca::test::has_resource("no_such_sample.txt"));

    EXPECT_EQ(as_string(ca::test::resource("sample.txt")), "sample resource\n");
    EXPECT_EQ(ca::test::resource_path("sample.txt").filename().string(), "sample.txt");
    EXPECT_THROW(ca::test::resource("no_such_sample.txt"), std::runtime_error);
}

TEST(TestLibTest, CrossProjectResourceByName)
{
    ca::test::setup("libca_test");

    EXPECT_TRUE(ca::test::has_project_resource("fake_proj", "cross.txt"));
    EXPECT_EQ(as_string(ca::test::project_resource("fake_proj", "cross.txt")), "cross\n");
    EXPECT_EQ(ca::test::project_resource_path("fake_proj", "cross.txt").filename().string(),
              "cross.txt");
}

TEST(TestLibTest, UnregisteredNameFallsBackToTopSubdir)
{
    ca::test::setup("libca_test");

    EXPECT_FALSE(ca::test::has_project("no_such_project_xyz"));
    EXPECT_TRUE(ca::test::has_project("fake_proj"));
    EXPECT_TRUE(ca::test::has_project("libca_test"));

    const auto fallback = ca::test::project_path("no_such_project_xyz");
    EXPECT_EQ(fallback.filename().string(), "no_such_project_xyz");
}

TEST(TestLibTest, OutPathsUnderOverrideRoot)
{
    const auto overrideRoot =
        fs::temp_directory_path() / "libca_test_out" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    set_env("LIBCA_TEST_OUT_ROOT", overrideRoot.string().c_str());

    ca::test::setup("libca_test");

    EXPECT_EQ(fs::weakly_canonical(ca::test::out_path("a.bin").parent_path()),
              fs::weakly_canonical(overrideRoot));
    EXPECT_EQ(fs::weakly_canonical(ca::test::demo_out_path("b.bin").parent_path()),
              fs::weakly_canonical(overrideRoot / "demo"));
    EXPECT_EQ(fs::weakly_canonical(ca::test::temp_out_path("c.bin").parent_path()),
              fs::weakly_canonical(overrideRoot / "tmp"));
    EXPECT_TRUE(fs::exists(overrideRoot / "demo"));
    EXPECT_TRUE(fs::exists(overrideRoot / "tmp"));

    set_env("LIBCA_TEST_OUT_ROOT", "");
    fs::remove_all(overrideRoot);
}
