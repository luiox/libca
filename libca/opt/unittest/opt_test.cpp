#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "libca/core/status.hpp"
#include "libca/opt/opt.hpp"

namespace ca::opt::test {
namespace {

using ca::core::StatusCode;
using ca::core::StatusResult;

// 持有参数字符串及其 argv 视图，避免悬垂指针（字符串生命周期与视图一致）。
struct Argv
{
    int                     argc;
    std::vector<std::string> storage;  // 持有 prog + args
    std::vector<const char*> argv;

    const char* const* data() const { return argv.data(); }
};

Argv make_argv(const std::vector<std::string>& args)
{
    Argv a;
    a.storage.emplace_back("prog");
    for (const auto& s : args)
        a.storage.push_back(s);
    for (const auto& s : a.storage)
        a.argv.push_back(s.c_str());
    a.argc = static_cast<int>(a.argv.size());
    return a;
}

// C++17 兼容的 Arg 构造辅助（不用 C++20 指定初始化器）。
Arg flag(std::string name, char short_name, std::string help = "")
{
    Arg a;
    a.name       = std::move(name);
    a.short_name = short_name;
    a.help       = std::move(help);
    return a;
}

Arg opt(std::string name, char short_name, std::string help = "", std::string defv = "",
        bool required = false)
{
    Arg a;
    a.name          = std::move(name);
    a.short_name    = short_name;
    a.help          = std::move(help);
    a.has_value     = true;
    a.default_value = std::move(defv);
    a.required      = required;
    return a;
}

Command make_root_with_options()
{
    Command root;
    root.name = "prog";
    root.help = "a test program";
    root.args = {
        flag("verbose", 'v', "verbose output"),
        opt("output", 'o', "output file"),
        opt("count", 'n', "count", "1"),
    };
    return root;
}

// 验收标准：解析 -v --verbose --name value --name=value 等常见形态。
TEST(OptParseTest, ShortFlag)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-v"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    EXPECT_TRUE(result.has("verbose"));
    EXPECT_EQ(result.get("verbose"), "true");
}

TEST(OptParseTest, LongFlag)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--verbose"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    EXPECT_TRUE(r.unwrap().has("verbose"));
}

TEST(OptParseTest, LongOptionWithSpaceValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--output", "file.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    EXPECT_EQ(r.unwrap().get("output"), "file.txt");
}

TEST(OptParseTest, LongOptionWithEqualsValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--output=file.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    EXPECT_EQ(r.unwrap().get("output"), "file.txt");
}

TEST(OptParseTest, ShortOptionWithSpaceValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-o", "file.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    EXPECT_EQ(r.unwrap().get("output"), "file.txt");
}

TEST(OptParseTest, ShortOptionWithAttachedValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-ofile.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    EXPECT_EQ(r.unwrap().get("output"), "file.txt");
}

TEST(OptParseTest, CombinedShortFlags)
{
    // -v 加另一个 flag 的组合：这里 verbose + 假设有第二个布尔 flag。
    Command root;
    root.name = "prog";
    root.args = {
        flag("alpha", 'a'),
        flag("beta", 'b'),
        flag("gamma", 'g'),
    };
    Parser p(root);
    Argv argv = make_argv({"-abg"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    EXPECT_TRUE(result.has("alpha"));
    EXPECT_TRUE(result.has("beta"));
    EXPECT_TRUE(result.has("gamma"));
}

TEST(OptParseTest, DefaultAppliedWhenAbsent)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-v"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.unwrap().get("count"), "1");  // 默认值
}

TEST(OptParseTest, DefaultOverriddenWhenProvided)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-n", "5"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.unwrap().get("count"), "5");
}

// 验收标准：required 选项缺失时返回明确错误。
TEST(OptParseTest, MissingRequiredReturnsError)
{
    Command root;
    root.name = "prog";
    root.args = {
        opt("input", 'i', "input", "", true),
    };
    Parser p(root);
    Argv argv = make_argv({});  // 不提供 -i

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    auto st = r.unwrap_err();
    EXPECT_EQ(st.code(), StatusCode::INVALID_ARGUMENT);
    EXPECT_NE(st.message().find("required"), std::string::npos);
    EXPECT_NE(st.message().find("input"), std::string::npos);
}

TEST(OptParseTest, RequiredSatisfied)
{
    Command root;
    root.name = "prog";
    root.args = {
        opt("input", 'i', "input", "", true),
    };
    Parser p(root);
    Argv argv = make_argv({"-i", "data.bin"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    EXPECT_EQ(r.unwrap().get("input"), "data.bin");
}

// 验收标准：子命令嵌套解析。
TEST(OptParseTest, SubcommandDispatch)
{
    Command root;
    root.name = "git";
    Command commit;
    commit.name = "commit";
    commit.help = "record changes";
    commit.args = {opt("message", 'm', "msg")};
    root.subcommands = {commit};

    Parser p(root);
    Argv argv = make_argv({"commit", "-m", "hello"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    ASSERT_EQ(result.subcommand_path().size(), 1u);
    EXPECT_EQ(result.subcommand_path()[0], "commit");
    EXPECT_EQ(result.get("message"), "hello");
}

TEST(OptParseTest, NestedSubcommands)
{
    Command root;
    root.name = "git";
    Command remote;
    remote.name = "remote";
    Command add;
    add.name = "add";
    add.args = {opt("name", 'n', "n", "", true)};
    remote.subcommands = {add};
    root.subcommands = {remote};

    Parser p(root);
    Argv argv = make_argv({"remote", "add", "-n", "origin"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    ASSERT_EQ(result.subcommand_path().size(), 2u);
    EXPECT_EQ(result.subcommand_path()[0], "remote");
    EXPECT_EQ(result.subcommand_path()[1], "add");
    EXPECT_EQ(result.get("name"), "origin");
}

// 验收标准：-- 正确截断选项解析。
TEST(OptParseTest, DoubleDashTerminatesOptions)
{
    Parser p(make_root_with_options());
    // --verbose 出现，-- 之后的内容不解析为选项。
    Argv argv = make_argv({"--verbose", "--", "--output", "should-not-be-parsed"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    EXPECT_TRUE(result.has("verbose"));
    EXPECT_FALSE(result.has("output"));  // 在 -- 之后，被当作位置参数忽略
}

// 验收标准：--help 输出格式化帮助文本。
TEST(OptParseTest, HelpReturnsCancelledStatusWithHelpText)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--help"});

    auto r = p.parse(argv.argc, argv.data());
    // --help 走 Err 分支，status code 为 CANCELLED（区别于真正的解析错误）。
    ASSERT_TRUE(r.is_err());
    auto st = r.unwrap_err();
    EXPECT_EQ(st.code(), StatusCode::CANCELLED);
    EXPECT_NE(st.message().find("Options:"), std::string::npos);
    EXPECT_NE(st.message().find("--verbose"), std::string::npos);
}

TEST(OptParseTest, ShortHelpFlag)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-h"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::CANCELLED);
}

TEST(OptParseTest, UnknownLongOptionIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--nope"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
}

TEST(OptParseTest, UnknownShortOptionIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-z"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
}

TEST(OptParseTest, ValueOptionWithoutValueAtEndIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--output"});  // 缺值

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
}

TEST(OptParseTest, FlagGivenInlineValueIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--verbose=yes"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
}

}  // namespace
}  // namespace ca::opt::test
