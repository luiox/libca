// libca.resources 端到端单测：rule 嵌入两组 fixture(crt/extra)，验证
// 查找/迭代/过滤/隔离/自注册全语义。fixture 树见 unittest/resources/：
//   crt/：top.txt、a/b/deep.bin、a/b/c/leaf.txt、dup.txt、empty.txt、
//         中文文件.txt、with space.txt、my dir/nested.txt、empty_dir/(空目录，应被丢弃)
//   extra/：dup.txt、only.txt（与 crt 同名路径，验证 bundle 隔离）

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "libca/resources/resources.hpp"

#include "resources_libca_resources_unittest.generated.hpp"

namespace {

/// 取字节视图为 std::string 便于比较。
std::string as_string(const ca::core::ByteSlice& slice)
{
    return std::string{reinterpret_cast<const char*>(slice.data()), slice.size()};
}

}   // namespace

TEST(ResourcesLookup, BundleFoundAndMissing)
{
    EXPECT_NE(ca::resources::bundle("crt"), nullptr);
    EXPECT_NE(ca::resources::bundle("extra"), nullptr);
    EXPECT_EQ(ca::resources::bundle("missing"), nullptr);
}

TEST(ResourcesLookup, FileCount)
{
    // empty_dir/ 无文件不计数。
    EXPECT_EQ(ca::resources::bundle("crt")->size(), 8U);
    EXPECT_EQ(ca::resources::bundle("extra")->size(), 2U);
}

TEST(ResourcesGet, Hit)
{
    const auto result = ca::resources::bundle("crt")->get("/top.txt");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(as_string(result.unwrap()), "top-level\n");
}

TEST(ResourcesGet, MissAndInvalidPath)
{
    const auto miss = ca::resources::bundle("crt")->get("/no/such/file.txt");
    ASSERT_TRUE(miss.is_err());
    EXPECT_EQ(miss.unwrap_err(), ca::resources::ResourceError::NotFound);

    // 未以 '/' 开头：非法路径，与未命中区分。
    const auto invalid = ca::resources::bundle("crt")->get("top.txt");
    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err(), ca::resources::ResourceError::InvalidPath);

    // '\' 分隔符同样拒绝（生成期已归一化，查询侧保持一致）。
    const auto backslash = ca::resources::bundle("crt")->get("\\top.txt");
    ASSERT_TRUE(backslash.is_err());
    EXPECT_EQ(backslash.unwrap_err(), ca::resources::ResourceError::InvalidPath);
}

TEST(ResourcesGet, BinaryBytesExact)
{
    const auto result = ca::resources::bundle("crt")->get("/a/b/deep.bin");
    ASSERT_TRUE(result.is_ok());
    // 含 NUL 与高位字节，验证二进制透明。
    EXPECT_EQ(as_string(result.unwrap()), (std::string{"\x00\x01\xFF\xFE\x00""A", 6}));
}

TEST(ResourcesGet, ZeroByteFile)
{
    ASSERT_TRUE(ca::resources::bundle("crt")->exists("/empty.txt"));
    const auto result = ca::resources::bundle("crt")->get("/empty.txt");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.unwrap().empty());
}

TEST(ResourcesGet, SpaceInName)
{
    // 文件名与目录名含空格：路径原样字节匹配，不做任何折叠。
    ASSERT_TRUE(ca::resources::bundle("crt")->exists("/with space.txt"));
    const auto file = ca::resources::bundle("crt")->get("/with space.txt");
    ASSERT_TRUE(file.is_ok());
    EXPECT_EQ(as_string(file.unwrap()), "space file\n");

    ASSERT_TRUE(ca::resources::bundle("crt")->exists("/my dir/nested.txt"));
    const auto nested = ca::resources::bundle("crt")->get("/my dir/nested.txt");
    ASSERT_TRUE(nested.is_ok());
    EXPECT_EQ(as_string(nested.unwrap()), "nested in spaced dir\n");
}

TEST(ResourcesGet, Utf8FilenameRoundTrip)
{
    const std::string path = "/\xE4\xB8\xAD\xE6\x96\x87\xE6\x96\x87\xE4\xBB\xB6.txt";
    ASSERT_TRUE(ca::resources::bundle("crt")->exists(path));
    const auto result = ca::resources::bundle("crt")->get(path);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(as_string(result.unwrap()), "\xE4\xB8\xAD\xE6\x96\x87\xE5\x86\x85\xE5\xAE\xB9\n");
}

TEST(ResourcesExists, HitMissInvalid)
{
    EXPECT_TRUE(ca::resources::bundle("crt")->exists("/dup.txt"));
    EXPECT_FALSE(ca::resources::bundle("crt")->exists("/only.txt"));
    EXPECT_FALSE(ca::resources::bundle("crt")->exists("dup.txt"));
}

TEST(ResourcesIterate, FullRangeOrderedAndComplete)
{
    const std::vector<std::string> expected = {
        "/a/b/c/leaf.txt",
        "/a/b/deep.bin",
        "/dup.txt",
        "/empty.txt",
        "/my dir/nested.txt",
        "/top.txt",
        "/with space.txt",
        "/\xE4\xB8\xAD\xE6\x96\x87\xE6\x96\x87\xE4\xBB\xB6.txt",  // UTF-8 字节序在 ASCII 之后
    };
    std::vector<std::string> actual;
    for (const auto& item : *ca::resources::bundle("crt")) {
        actual.emplace_back(item.path);
    }
    EXPECT_EQ(actual, expected);
}

TEST(ResourcesIterate, ItemBytesAccessor)
{
    for (const auto& item : *ca::resources::bundle("extra")) {
        if (item.path == "/only.txt") {
            EXPECT_EQ(as_string(item.bytes()), "only\n");
            return;
        }
    }
    FAIL() << "/only.txt not visited";
}

TEST(ResourcesIterate, UnderPrefixFilter)
{
    std::vector<std::string> paths;
    for (const auto& item : ca::resources::bundle("crt")->under("/a/")) {
        paths.emplace_back(item.path);
    }
    EXPECT_EQ(paths, (std::vector<std::string>{"/a/b/c/leaf.txt", "/a/b/deep.bin"}));
}

TEST(ResourcesIterate, UnderWithoutTrailingSlashEquivalent)
{
    // 缺尾 '/' 自动补齐语义：不得误吞 "/ab" 类兄弟前缀。
    const auto range_a = ca::resources::bundle("crt")->under("/a");
    const auto range_b = ca::resources::bundle("crt")->under("/a/");
    EXPECT_EQ(range_a.size(), range_b.size());

    // "/d" 不吞 "/dup.txt"（它属于根而非 "/d/" 目录）。
    EXPECT_TRUE(ca::resources::bundle("crt")->under("/d").empty());
    EXPECT_EQ(ca::resources::bundle("crt")->under("/d").size(),
              ca::resources::bundle("crt")->under("/d/").size());
}

TEST(ResourcesIterate, UnderRootIsAllAndEmptyDirInvisible)
{
    EXPECT_EQ(ca::resources::bundle("crt")->under("/").size(),
              ca::resources::bundle("crt")->size());
    EXPECT_EQ(ca::resources::bundle("crt")->under("").size(),
              ca::resources::bundle("crt")->size());
    for (const auto& item : *ca::resources::bundle("crt")) {
        EXPECT_EQ(item.path.find("/empty_dir/"), std::string_view::npos) << "empty dir leaked";
    }
}

TEST(ResourcesIsolation, SamePathDifferentBundles)
{
    const auto in_crt   = ca::resources::bundle("crt")->get("/dup.txt");
    const auto in_extra = ca::resources::bundle("extra")->get("/dup.txt");
    ASSERT_TRUE(in_crt.is_ok());
    ASSERT_TRUE(in_extra.is_ok());
    EXPECT_EQ(as_string(in_crt.unwrap()), "in crt\n");
    EXPECT_EQ(as_string(in_extra.unwrap()), "in extra\n");

    EXPECT_FALSE(ca::resources::bundle("crt")->exists("/only.txt"));
}

TEST(ResourcesRegistry, DuplicateMountKeepsFirst)
{
    static const ca::resources::RawEntry kEntries[] = {{"/x.txt", nullptr, 0}};
    static const ca::resources::Bundle   kOther{kEntries, 1};

    EXPECT_FALSE(ca::resources::mount("extra", kOther));  // 撞名：拒绝
    const auto* kept = ca::resources::bundle("extra");
    ASSERT_NE(kept, nullptr);
    EXPECT_EQ(kept->size(), 2U);  // 仍是原 fixture 包

    // 未占用名可注册。
    EXPECT_TRUE(ca::resources::mount("fresh", kOther));
    EXPECT_EQ(ca::resources::bundle("fresh"), &kOther);
}
