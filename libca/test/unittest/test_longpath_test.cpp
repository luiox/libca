// libca.test 启动期扫描长路径目录回归（morpher issue #1126 同族）。
//
// 症状：启动期扫描可见的目录树（如共享测试临时根）下存在 ≥260 字符（UTF-16
// 计；传统 MAX_PATH 上限 259）目录时，首个 setup() 的启动期递归枚举对该目录
// 的 status/深入调用失败，以未捕获 std::filesystem 异常终结进程（非测试宿主
// 即 0xC0000409 硬崩）。
//
// 本文件纯自造 fixture（libca 无样本罐，零拷贝）：
//   <CWD>/libca_it1126_longpath/中文长路径测试目录_<i>_ab…/…/.project_root_file
// 叶子路径总长 ≥300 UTF-16 单位，目录名含 CJK；修复前证红，修复后断言：
// 不崩 + 行为正确（深层 .project_root_file 被枚举到并注册为项目）。
//
// fixture 的构造/清理自身经 \\?\ 扩展长度宽字符链路完成，不依赖测试进程
// 携带 longPathAware 清单；被测逻辑（ca::test::setup 启动期枚举）保持走
// 常规路径，保证回归的真实性。
//
// fixture 由全局 Environment 在全部用例前构造：启动期扫描 call_once 只发生
// 一次，必须先于首个 setup() 就位；TearDown 尽力清理，不污染工作区。
// （经静态初始化注册 Environment，既有 unittest/main.cpp 零改动。）

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

#include <libca/test/test.hpp>

namespace {

namespace fs = std::filesystem;

constexpr wchar_t     kFixtureDirName[] = L"libca_it1126_longpath";
constexpr const char* kFixtureProject   = "longpath_it1126";
constexpr std::size_t kMinLeafLength    = 260;   // > 传统 MAX_PATH 上限 259
constexpr std::size_t kTargetLeafLength = 300;   // 留出余量

/// 扩展长度（\\?\）路径：仅用于 fixture 构造/清理。
fs::path extended_path(const fs::path& path)
{
    std::wstring wide = path.wstring();
    if (wide.rfind(L"\\\\?\\", 0) == 0) return path;
    if (!path.is_absolute()) return path;
    std::replace(wide.begin(), wide.end(), L'/', L'\\');
    return fs::path(L"\\\\?\\" + wide);
}

/// 第 index 层目录名：CJK + ASCII 填充（UTF-16 计 ~37 单位）。
std::wstring segment_name(std::size_t index)
{
    std::wstring name = L"中文长路径测试目录_";
    name += std::to_wstring(index);
    name += L"_abcdefghijklmnopqrstuvwx";
    return name;
}

/// fixture 根：<CWD>/<固定名>。CWD = libca/test（xmake set_rundir）。
fs::path fixture_root()
{
    return fs::current_path() / kFixtureDirName;
}

/// 确定性推导叶子目录：自根逐层拼接段名直至总长达标。
fs::path expected_leaf()
{
    fs::path leaf = fixture_root();
    for (std::size_t index = 0; leaf.wstring().size() < kTargetLeafLength; ++index) {
        leaf /= segment_name(index);
    }
    return leaf;
}

/// 逐层创建长路径树（已存在忽略，容忍上次硬崩残留），返回叶子目录路径。
fs::path build_fixture_tree()
{
    const fs::path base = fixture_root();
    std::error_code ec;
    fs::create_directories(extended_path(base), ec);
    ec.clear();

    fs::path cumulative = base;
    for (std::size_t index = 0; cumulative.wstring().size() < kTargetLeafLength; ++index) {
        cumulative /= segment_name(index);
        fs::create_directory(extended_path(cumulative), ec);
        ec.clear();
    }
    return cumulative;
}

void write_marker_file(const fs::path& leaf)
{
    std::ofstream marker(extended_path(leaf / L".project_root_file"),
                         std::ios::binary | std::ios::trunc);
    marker << kFixtureProject << "\n";
}

class LongPathFixtureEnvironment : public ::testing::Environment {
public:
    void SetUp() override { write_marker_file(build_fixture_tree()); }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(extended_path(fixture_root()), ec);
    }
};

[[maybe_unused]] ::testing::Environment* const g_longpath_fixture =
    ::testing::AddGlobalTestEnvironment(new LongPathFixtureEnvironment);

}   // namespace

TEST(TestLibLongPath, StartupScanSurvivesLongCjkTempDirs)
{
    const fs::path leaf = expected_leaf();

    // fixture 自检：叶子路径长超过传统 MAX_PATH 上限、含 CJK 且确已就位。
    ASSERT_GE(leaf.wstring().size(), kMinLeafLength);
    ASSERT_TRUE(leaf.wstring().find(L"中文") != std::wstring::npos)
        << "fixture 应含 CJK 目录名";
    ASSERT_TRUE(fs::exists(extended_path(leaf)))
        << "fixture 缺失：" << extended_path(leaf).string();

    // 启动期扫描：修复前此调用即抛未捕获 filesystem 异常（硬崩/用例红）。
    ca::test::setup("libca_test");
    EXPECT_TRUE(ca::test::has_project("libca_test"));

    // 行为正确：深层 marker 被枚举到并注册为项目，注册路径与 fixture 一致。
    EXPECT_TRUE(ca::test::has_project(kFixtureProject));
    EXPECT_EQ(ca::test::project_path(kFixtureProject).wstring(), leaf.wstring());
}
