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
    a.name    = std::move(name);
    a.kind    = OptKind::Flag;
    a.help    = std::move(help);
    if (short_name != 0) a.aliases = {std::string("-") + short_name};
    return a;
}

Arg opt(std::string name, char short_name, std::string help = "", std::string defv = "",
        bool required = false)
{
    Arg a;
    a.name          = std::move(name);
    a.kind          = OptKind::String;
    a.help          = std::move(help);
    if (short_name != 0) a.aliases = {std::string("-") + short_name};
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

// 验收标准：-- 正确截断选项解析；-- 之后的 token 收集进 positionals()（v2 行为）。
TEST(OptParseTest, DoubleDashTerminatesOptions)
{
    Parser p(make_root_with_options());
    // --verbose 出现，-- 之后的内容不解析为选项，而是全部进入位置参数。
    Argv argv = make_argv({"--verbose", "--", "--output", "plain.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    EXPECT_TRUE(result.has("verbose"));
    EXPECT_FALSE(result.has("output"));  // 在 -- 之后，不再解析为选项
    ASSERT_EQ(result.positionals().size(), 2u);
    EXPECT_EQ(result.positionals()[0], "--output");
    EXPECT_EQ(result.positionals()[1], "plain.txt");
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

// name 是 has()/get() 的唯一 key，必须非空；空 name 在 parse 时报错。
TEST(OptParseTest, EmptyNameIsRejected)
{
    Command root;
    root.name = "prog";
    Arg a;
    a.aliases = {"-v"};
    a.name    = "";  // 空 name，仅别名 —— 应被拒绝
    a.kind    = OptKind::Flag;
    root.args = {a};
    Parser p(root);
    Argv argv = make_argv({"-v"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
}

// required 与 default_value 互斥：有默认值时 required 永不触发，应在 parse 时报错。
TEST(OptParseTest, RequiredWithDefaultIsRejected)
{
    Command root;
    root.name = "prog";
    // has_value + default_value + required 同时为真 —— 语义矛盾，应拒绝。
    root.args = {opt("cfg", 'c', "config", "default.ini", true)};
    Parser p(root);
    Argv argv = make_argv({"-c", "x.ini"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
    EXPECT_NE(r.unwrap_err().message().find("default_value"), std::string::npos);
}

// 根命令定义了 required 选项，用户直接进入子命令而未提供该 required 时应报错
// （修复"根级 required 在子命令分派后被静默跳过"的回归）。
TEST(OptParseTest, RootRequiredNotSkippedWhenEnteringSubcommand)
{
    Command root;
    root.name = "git";
    root.args = {opt("repo", 'r', "repo path", "", true)};  // 根级 required
    Command commit;
    commit.name = "commit";
    commit.args = {opt("message", 'm', "msg")};
    root.subcommands = {commit};

    Parser p(root);
    // 进入 commit 但没提供根级 -r，应报根级 required 缺失。
    Argv argv = make_argv({"commit", "-m", "hello"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
    EXPECT_NE(r.unwrap_err().message().find("repo"), std::string::npos);
}

// 根级 required 与子命令同时满足时应正常解析。
TEST(OptParseTest, RootRequiredAndSubcommandBothSatisfied)
{
    Command root;
    root.name = "git";
    root.args = {opt("repo", 'r', "repo path", "", true)};
    Command commit;
    commit.name = "commit";
    commit.args = {opt("message", 'm', "msg")};
    root.subcommands = {commit};

    Parser p(root);
    Argv argv = make_argv({"-r", "/p", "commit", "-m", "hi"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    EXPECT_EQ(result.get("repo"), "/p");
    EXPECT_EQ(result.get("message"), "hi");
    ASSERT_EQ(result.subcommand_path().size(), 1u);
    EXPECT_EQ(result.subcommand_path()[0], "commit");
}

// ==========================================================================
// v2 P0：位置参数、类型化、多别名、重复语义、带默认取值
// ==========================================================================

// 声明 Positional 后，裸 token 按顺序收集；与选项混排不冲突。
TEST(OptV2Test, PositionalCollectedInOrder)
{
    Arg positional;
    positional.name = "pattern";
    positional.kind = OptKind::Positional;
    positional.help = "search pattern";

    Arg case_flag;
    case_flag.name = "ignore-case";
    case_flag.kind = OptKind::Flag;

    Command root;
    root.name = "grep";
    root.args = {positional, case_flag};

    Parser p(root);
    Argv argv = make_argv({"hello", "--ignore-case", "world"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();
    ASSERT_EQ(result.positionals().size(), 2u);
    EXPECT_EQ(result.positionals()[0], "hello");
    EXPECT_EQ(result.positionals()[1], "world");
    EXPECT_TRUE(result.has("ignore-case"));
}

// 未声明 Positional 时的多余裸 token：报错而非静默丢弃（v2 行为变更）。
TEST(OptV2Test, UnexpectedPositionalIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"stray"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().code(), StatusCode::INVALID_ARGUMENT);
    EXPECT_NE(r.unwrap_err().message().find("unexpected argument"), std::string::npos);
}

// 多别名（含短名）：任意别名命中同一 canonical key。
TEST(OptV2Test, AliasedOptionCanonicalKey)
{
    Arg input;
    input.name    = "input";
    input.aliases = {"-i", "--in"};
    input.kind    = OptKind::String;
    input.help    = "input file";

    Command root;
    root.name = "prog";
    root.args = {input};

    // 短别名
    {
        Parser p(root);
        Argv argv = make_argv({"-i", "a.jar"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
        EXPECT_EQ(r.unwrap().get("input"), "a.jar");
    }
    // 长别名
    {
        Parser p(root);
        Argv argv = make_argv({"--in=b.jar"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
        EXPECT_EQ(r.unwrap().get("input"), "b.jar");
    }
}

// Int 类型化：合法值 get_int 取回；非法整数在 parse 阶段报错。
TEST(OptV2Test, IntTypeValidAndInvalid)
{
    Arg timeout;
    timeout.name = "timeout";
    timeout.kind = OptKind::Int;
    timeout.help = "seconds";

    Command root;
    root.name = "prog";
    root.args = {timeout};

    {
        Parser p(root);
        Argv argv = make_argv({"--timeout", "120"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
        EXPECT_EQ(r.unwrap().get_int("timeout"), 120);
    }
    {
        Parser p(root);
        Argv argv = make_argv({"--timeout=12abc"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_NE(r.unwrap_err().message().find("expects an integer"), std::string::npos);
    }
    // 负数与溢出边界
    {
        Parser p(root);
        Argv argv = make_argv({"--timeout", "-5"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
        EXPECT_EQ(r.unwrap().get_int("timeout"), -5);
    }
    {
        Parser p(root);
        Argv argv = make_argv({"--timeout", "99999999999999"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
    }
}

// StringList：逗号拆分与多次出现追加，按序合并。
TEST(OptV2Test, StringListCommaAndRepeated)
{
    Arg pass;
    pass.name    = "define";
    pass.aliases = {"-D"};
    pass.kind    = OptKind::StringList;
    pass.help    = "defines";

    Command root;
    root.name = "prog";
    root.args = {pass};

    Parser p(root);
    Argv argv = make_argv({"--define=A,B", "-DC"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto list = r.unwrap().get_list("define");
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0], "A");
    EXPECT_EQ(list[1], "B");
    EXPECT_EQ(list[2], "C");
}

// String last-wins；带默认的取值重载仅在未出现时生效。
TEST(OptV2Test, LastWinsAndDefaultOverloads)
{
    Arg output;
    output.name          = "output";
    output.kind          = OptKind::String;
    output.default_value = "out.bin";

    Arg level;
    level.name = "level";
    level.kind = OptKind::Int;

    Command root;
    root.name = "prog";
    root.args = {output, level};

    Parser p(root);
    Argv argv = make_argv({"--output", "a.bin", "--output", "b.bin"});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message();
    auto result = r.unwrap();

    // last-wins
    EXPECT_EQ(result.get("output"), "b.bin");
    // get(name, def)：出现时返回实际值
    EXPECT_EQ(result.get("output", "fallback"), "b.bin");
    // 未出现的 Int 选项：get_int 返回调用方默认
    EXPECT_EQ(result.get_int("level", 7), 7);
    // String 默认值已预置：has() 为真且 get 返回默认
    EXPECT_TRUE(result.has("output"));
}

}  // namespace
}  // namespace ca::opt::test
