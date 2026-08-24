#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "libca/opt/opt.hpp"

namespace ca::opt::test {
namespace {

using ca::opt::Arg;
using ca::opt::Command;
using ca::opt::MutexGroup;
using ca::opt::OptKind;
using ca::opt::ParseErrorCategory;
using ca::opt::Parser;

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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    auto result = r.unwrap();
    EXPECT_TRUE(result.has("verbose"));
    EXPECT_EQ(result.get("verbose"), "true");
}

TEST(OptParseTest, LongFlag)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--verbose"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    EXPECT_TRUE(r.unwrap().has("verbose"));
}

TEST(OptParseTest, LongOptionWithSpaceValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--output", "file.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    EXPECT_EQ(r.unwrap().get("output"), "file.txt");
}

TEST(OptParseTest, LongOptionWithEqualsValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--output=file.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    EXPECT_EQ(r.unwrap().get("output"), "file.txt");
}

TEST(OptParseTest, ShortOptionWithSpaceValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-o", "file.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    EXPECT_EQ(r.unwrap().get("output"), "file.txt");
}

TEST(OptParseTest, ShortOptionWithAttachedValue)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-ofile.txt"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    EXPECT_EQ(st.category, ParseErrorCategory::MissingRequired);
    EXPECT_EQ(st.option, "input");
    EXPECT_NE(st.message.find("required"), std::string::npos);
    EXPECT_NE(st.message.find("input"), std::string::npos);
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    // --help 走 Err 分支，category 为 HelpRequested（区别于真正的解析错误）。
    ASSERT_TRUE(r.is_err());
    auto st = r.unwrap_err();
    EXPECT_EQ(st.category, ParseErrorCategory::HelpRequested);
    EXPECT_NE(st.message.find("Options:"), std::string::npos);
    EXPECT_NE(st.message.find("--verbose"), std::string::npos);
}

TEST(OptParseTest, ShortHelpFlag)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-h"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::HelpRequested);
}

TEST(OptParseTest, UnknownLongOptionIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--nope"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::UnknownOption);
}

TEST(OptParseTest, UnknownShortOptionIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"-z"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::UnknownOption);
}

TEST(OptParseTest, ValueOptionWithoutValueAtEndIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--output"});  // 缺值

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MissingValue);
    EXPECT_EQ(r.unwrap_err().option, "output");
}

TEST(OptParseTest, FlagGivenInlineValueIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--verbose=yes"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::UnexpectedArgument);
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
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::InvalidDefinition);
}

// required 与 default_value 互斥：有默认值时 required 永不触发，应在 parse 时报错。
TEST(OptParseTest, RequiredWithDefaultIsRejected)
{
    Command root;
    root.name = "prog";
    // required + default_value 同时为真 —— 语义矛盾，应拒绝。
    root.args = {opt("cfg", 'c', "config", "default.ini", true)};
    Parser p(root);
    Argv argv = make_argv({"-c", "x.ini"});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::InvalidDefinition);
    EXPECT_NE(r.unwrap_err().message.find("default_value"), std::string::npos);
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
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MissingRequired);
    EXPECT_NE(r.unwrap_err().message.find("repo"), std::string::npos);
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::UnexpectedArgument);
    EXPECT_NE(r.unwrap_err().message.find("unexpected argument"), std::string::npos);
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
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("input"), "a.jar");
    }
    // 长别名
    {
        Parser p(root);
        Argv argv = make_argv({"--in=b.jar"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get_int("timeout"), 120);
    }
    {
        Parser p(root);
        Argv argv = make_argv({"--timeout=12abc"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::InvalidInteger);
        EXPECT_EQ(r.unwrap_err().option, "timeout");
        EXPECT_NE(r.unwrap_err().message.find("expects an integer"), std::string::npos);
    }
    // 负数与溢出边界
    {
        Parser p(root);
        Argv argv = make_argv({"--timeout", "-5"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
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

// ==========================================================================
// v2 P1：错误分类、互斥组、选项分组渲染、自定义 usage
// ==========================================================================

// 带值选项收到空值（--name= 形式）报 EmptyValue。
TEST(OptV2P1Test, EmptyInlineValueIsError)
{
    Parser p(make_root_with_options());
    Argv argv = make_argv({"--output="});

    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::EmptyValue);
    EXPECT_EQ(r.unwrap_err().option, "output");
}

// 互斥组：两个成员同时出现报 MutexConflict。
TEST(OptV2P1Test, MutexConflictRejected)
{
    Arg json;
    json.name = "json";
    json.kind = OptKind::Flag;
    Arg yaml;
    yaml.name = "yaml";
    yaml.kind = OptKind::Flag;

    Command root;
    root.name         = "prog";
    root.args         = {json, yaml};
    root.mutex_groups = {{{"json", "yaml"}, false}};

    Parser p(root);
    Argv argv = make_argv({"--json", "--yaml"});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexConflict);
    // 无 label 的组：错误标识为成员名拼接。
    EXPECT_EQ(r.unwrap_err().group, "json|yaml");
    EXPECT_NE(r.unwrap_err().message.find("--json"), std::string::npos);
    // 只出现一个成员时合法。
    Parser p2(root);
    Argv argv2 = make_argv({"--yaml"});
    auto r2 = p2.parse(argv2.argc, argv2.data());
    ASSERT_TRUE(r2.is_ok()) << r2.unwrap_err().message;
    EXPECT_TRUE(r2.unwrap().has("yaml"));
}

// 带 label 的互斥组：错误标识回填 label，供下游按组分派文案。
TEST(OptV2P1Test, MutexGroupLabelInError)
{
    Arg schema;
    schema.name = "dump-config-schema";
    schema.kind = OptKind::OptionalString;
    Arg tmpl;
    tmpl.name = "dump-config-template";
    tmpl.kind = OptKind::OptionalString;

    Command root;
    root.name = "prog";
    root.args = {schema, tmpl};
    MutexGroup once;
    once.names  = {"dump-config-schema", "dump-config-template"};
    once.label  = "dump_once";
    root.mutex_groups = {once};

    {
        Parser p(root);
        Argv argv = make_argv({"--dump-config-schema", "--dump-config-template"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexConflict);
        EXPECT_EQ(r.unwrap_err().group, "dump_once");
    }
    {
        MutexGroup req = once;
        req.required   = true;
        Command root2  = root;
        root2.mutex_groups = {req};
        Parser p(root2);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexRequired);
        EXPECT_EQ(r.unwrap_err().group, "dump_once");
    }
}

// required 互斥组：全缺报 MutexRequired；出现其一即通过。
TEST(OptV2P1Test, MutexRequiredEnforced)
{
    Arg a;
    a.name = "tcp";
    a.kind = OptKind::Flag;
    Arg b;
    b.name = "unix";
    b.kind = OptKind::Flag;

    Command root;
    root.name         = "prog";
    root.args         = {a, b};
    root.mutex_groups = {{{"tcp", "unix"}, true}};

    Parser p(root);
    Argv argv = make_argv({});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexRequired);

    Parser p2(root);
    Argv argv2 = make_argv({"--tcp"});
    auto r2 = p2.parse(argv2.argc, argv2.data());
    ASSERT_TRUE(r2.is_ok()) << r2.unwrap_err().message;
}

// 互斥组引用未注册的选项名：InvalidDefinition。
TEST(OptV2P1Test, MutexGroupUnknownMemberIsInvalidDefinition)
{
    Arg a;
    a.name = "tcp";
    a.kind = OptKind::Flag;

    Command root;
    root.name         = "prog";
    root.args         = {a};
    root.mutex_groups = {{{"tcp", "nope"}, false}};

    Parser p(root);
    Argv argv = make_argv({"--tcp"});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::InvalidDefinition);
    EXPECT_NE(r.unwrap_err().message.find("nope"), std::string::npos);
}

// 进入子命令前父命令的互斥约束同样生效（不被分派跳过）。
TEST(OptV2P1Test, ParentMutexCheckedBeforeSubcommand)
{
    Arg json;
    json.name = "json";
    json.kind = OptKind::Flag;
    Arg xml;
    xml.name = "xml";
    xml.kind = OptKind::Flag;

    Command root;
    root.name         = "git";
    root.args         = {json, xml};
    root.mutex_groups = {{{"json", "xml"}, false}};
    Command show;
    show.name = "show";
    root.subcommands = {show};

    Parser p(root);
    Argv argv = make_argv({"--json", "--xml", "show"});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexConflict);
}

// help 渲染：分组标签成为节标题；互斥成员带标注。
TEST(OptV2P1Test, HelpRendersGroupsAndMutexAnnotation)
{
    Arg verbose;
    verbose.name = "verbose";
    verbose.kind = OptKind::Flag;
    verbose.help = "chatty";

    Arg input;
    input.name  = "input";
    input.kind  = OptKind::String;
    input.group = "Source";
    input.help  = "input file";

    Arg output;
    output.name  = "output";
    output.kind  = OptKind::String;
    output.group = "Sink";
    output.help  = "output file";

    Command root;
    root.name         = "prog";
    root.args         = {verbose, input, output};
    root.mutex_groups = {{{"input", "output"}, false}};

    Parser p(root);
    Argv argv = make_argv({"--help"});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    const std::string help = r.unwrap_err().message;
    // 默认节仍存在
    EXPECT_NE(help.find("\nOptions:\n"), std::string::npos);
    // 分组节标题按标签输出
    EXPECT_NE(help.find("\nSource:\n"), std::string::npos);
    EXPECT_NE(help.find("\nSink:\n"), std::string::npos);
    // 互斥标注
    EXPECT_NE(help.find("[exclusive]"), std::string::npos);
}

// 自定义 usage 行替换自动生成部分。
TEST(OptV2P1Test, CustomUsageLine)
{
    Command root;
    root.name  = "mj2x";
    root.usage = "mj2x [--backend <name>] <jar> <out>";
    root.help  = "convert jars";

    Parser p(root);
    Argv argv = make_argv({"--help"});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    const std::string help = r.unwrap_err().message;
    EXPECT_NE(help.find("Usage: mj2x [--backend <name>] <jar> <out>\n"), std::string::npos);
    // 自定义 usage 下不再追加自动生成的 "[options]"。
    EXPECT_EQ(help.find("Usage: mj2x [options]"), std::string::npos);
}

// to_status_code 桥接：HelpRequested -> CANCELLED，InvalidDefinition -> FAILED_PRECONDITION。
TEST(OptV2P1Test, ToStatusCodeBridge)
{
    using ca::opt::to_status_code;
    EXPECT_EQ(to_status_code(ParseErrorCategory::HelpRequested), ca::core::StatusCode::CANCELLED);
    EXPECT_EQ(to_status_code(ParseErrorCategory::InvalidDefinition),
              ca::core::StatusCode::FAILED_PRECONDITION);
    EXPECT_EQ(to_status_code(ParseErrorCategory::UnknownOption),
              ca::core::StatusCode::INVALID_ARGUMENT);
}

// ==========================================================================
// v2 P2：初始值注入（三级优先级）、元数据只读视图、分组过滤帮助
// ==========================================================================

// 三级优先级：静态 default < 注入初值 < 命令行显式出现。
TEST(OptV2P2Test, InitialValuesPrecedence)
{
    Arg output;
    output.name          = "output";
    output.kind          = OptKind::String;
    output.default_value = "from-default";

    Command root;
    root.name = "prog";
    root.args = {output};

    const std::unordered_map<std::string, std::string> initials = {{"output", "from-config"}};

    // 无 CLI：取注入初值。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data(), initials);
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("output"), "from-config");
    }
    // 有 CLI：CLI 覆盖注入初值。
    {
        Parser p(root);
        Argv argv = make_argv({"--output", "from-cli"});
        auto r = p.parse(argv.argc, argv.data(), initials);
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("output"), "from-cli");
    }
    // 无初值无 CLI：回落静态默认。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("output"), "from-default");
    }
}

// 注入初值的 Int 校验与空串拒绝；未知名字静默忽略；Flag 不参与注入。
TEST(OptV2P2Test, InitialValuesValidationAndScope)
{
    Arg timeout;
    timeout.name = "timeout";
    timeout.kind = OptKind::Int;

    Arg verbose;
    verbose.name = "verbose";
    verbose.kind = OptKind::Flag;

    Command root;
    root.name = "prog";
    root.args = {timeout, verbose};

    // 非法整数初值。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data(), {{"timeout", "12x"}});
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::InvalidInteger);
        EXPECT_EQ(r.unwrap_err().option, "timeout");
    }
    // 空串初值。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data(), {{"timeout", ""}});
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::EmptyValue);
    }
    // 未知名字忽略，合法整数生效，Flag 注入被忽略（保持未置位）。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data(),
                         {{"nope", "x"}, {"timeout", "42"}, {"verbose", "true"}});
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_EQ(result.get_int("timeout"), 42);
        EXPECT_FALSE(result.has("verbose"));
    }
}

// StringList 初值按逗号拆分追加，且与 CLI 出现按序合并。
TEST(OptV2P2Test, InitialValuesStringListAppend)
{
    Arg define;
    define.name    = "define";
    define.aliases = {"-D"};
    define.kind    = OptKind::StringList;

    Command root;
    root.name = "prog";
    root.args = {define};

    Parser p(root);
    Argv argv = make_argv({"-DC"});
    auto r = p.parse(argv.argc, argv.data(), {{"define", "A,B"}});
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    auto list = r.unwrap().get_list("define");
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0], "A");
    EXPECT_EQ(list[1], "B");
    EXPECT_EQ(list[2], "C");
}

// 进入子命令时同样按其选项名匹配注入初值；required 与互斥组把注入视为已提供。
TEST(OptV2P2Test, InitialValuesSubcommandAndConstraints)
{
    Arg message;
    message.name     = "message";
    message.aliases  = {"-m"};
    message.kind     = OptKind::String;
    message.required = true;

    Arg amend;
    amend.name = "amend";
    amend.kind = OptKind::Flag;
    // 互斥组成员须为带值选项才能被初值满足（Flag 不参与注入）。
    Arg gpg_sign;
    gpg_sign.name    = "gpg-sign";
    gpg_sign.kind    = OptKind::String;
    gpg_sign.metavar = "<key>";

    Command commit;
    commit.name         = "commit";
    commit.args         = {message, amend, gpg_sign};
    commit.mutex_groups = {{{"amend", "gpg-sign"}, true}};

    const std::unordered_map<std::string, std::string> initials = {{"message", "hi"},
                                                                   {"gpg-sign", "key-1"}};

    Command root;
    root.name        = "git";
    root.subcommands = {commit};

    Parser p(root);
    Argv argv = make_argv({"commit"});
    auto r = p.parse(argv.argc, argv.data(), initials);
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    auto result = r.unwrap();
    EXPECT_EQ(result.get("message"), "hi");
    // required 由初值满足；互斥组 required 也由初值满足；Flag 保持未置位。
    EXPECT_FALSE(result.has("amend"));
    EXPECT_TRUE(result.has("gpg-sign"));
}

// 元数据只读视图：root() 提供 const 访问供下游导出 schema。
TEST(OptV2P2Test, RootMetadataAccess)
{
    Arg input;
    input.name    = "input";
    input.aliases = {"-i"};
    input.kind    = OptKind::String;
    input.group   = "Source";
    input.help    = "input file";
    input.required = true;

    Command root;
    root.name  = "mj2x";
    root.usage = "mj2x [options] <jar>";
    root.args  = {input};

    const Parser p(root);
    const Command& meta = p.root();
    EXPECT_EQ(meta.name, "mj2x");
    EXPECT_EQ(meta.usage, "mj2x [options] <jar>");
    ASSERT_EQ(meta.args.size(), 1u);
    EXPECT_EQ(meta.args[0].name, "input");
    ASSERT_EQ(meta.args[0].aliases.size(), 1u);
    EXPECT_EQ(meta.args[0].aliases[0], "-i");
    EXPECT_EQ(meta.args[0].group, "Source");
    EXPECT_TRUE(meta.args[0].required);
}

// help_text：groups 为空渲染全部；指定 groups 时仅保留对应分组节，
// 未分组选项节省略，位置参数不受过滤影响。
TEST(OptV2P2Test, HelpTextGroupFilter)
{
    Arg verbose;
    verbose.name = "verbose";
    verbose.kind = OptKind::Flag;
    verbose.help = "chatty";

    Arg pattern;
    pattern.name = "pattern";
    pattern.kind = OptKind::Positional;
    pattern.help = "search text";

    Arg input;
    input.name  = "input";
    input.kind  = OptKind::String;
    input.group = "Source";
    input.help  = "input file";

    Arg output;
    output.name  = "output";
    output.kind  = OptKind::String;
    output.group = "Sink";
    output.help  = "output file";

    Command root;
    root.name = "prog";
    root.args = {verbose, pattern, input, output};

    // 全量渲染：默认节 + 两个分组节 + Arguments。
    const std::string full = ca::opt::help_text(root);
    EXPECT_NE(full.find("\nOptions:\n"), std::string::npos);
    EXPECT_NE(full.find("\nSource:\n"), std::string::npos);
    EXPECT_NE(full.find("\nSink:\n"), std::string::npos);
    EXPECT_NE(full.find("\nArguments:\n"), std::string::npos);

    // 仅 Source 分组：无默认节、无 Sink 节，Arguments 保留。
    const std::string filtered = ca::opt::help_text(root, {"Source"});
    EXPECT_EQ(filtered.find("\nOptions:\n"), std::string::npos);
    EXPECT_NE(filtered.find("\nSource:\n"), std::string::npos);
    EXPECT_EQ(filtered.find("\nSink:\n"), std::string::npos);
    EXPECT_NE(filtered.find("\nArguments:\n"), std::string::npos);
}

// ==========================================================================
// 值来源查询与互斥组判定修正：静态默认不构成「选择」
// ==========================================================================

using ca::opt::ValueSource;

// source_of 区分三种来源；CLI 覆盖注入后来源随之翻转为 CommandLine。
TEST(OptV2ProvenanceTest, SourceOfDistinguishesThreeOrigins)
{
    Arg output;
    output.name          = "output";
    output.kind          = OptKind::String;
    output.default_value = "from-default";

    Arg level;
    level.name = "level";
    level.kind = OptKind::Int;

    Command root;
    root.name = "prog";
    root.args = {output, level};

    // 无 CLI、有注入：output=Initial，level=None。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data(), {{"level", "3"}});
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_EQ(result.source_of("output"), ValueSource::Default);
        EXPECT_EQ(result.source_of("level"), ValueSource::Initial);
        EXPECT_EQ(result.source_of("nope"), ValueSource::None);
    }
    // CLI 显式给出：覆盖注入，来源翻转为 CommandLine。
    {
        Parser p(root);
        Argv argv = make_argv({"--output", "from-cli"});
        auto r = p.parse(argv.argc, argv.data(), {{"output", "from-config"}});
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_EQ(result.get("output"), "from-cli");
        EXPECT_EQ(result.source_of("output"), ValueSource::CommandLine);
    }
}

// 两个带默认值成员同组：空命令行不再误报冲突（静态默认不算选择）。
TEST(OptV2ProvenanceTest, MutexDefaultsNoFalseConflict)
{
    Arg json;
    json.name          = "format-json";
    json.kind          = OptKind::String;
    json.default_value = "pretty";

    Arg yaml;
    yaml.name          = "format-yaml";
    yaml.kind          = OptKind::String;
    yaml.default_value = "raw";

    Command root;
    root.name         = "prog";
    root.args         = {json, yaml};
    root.mutex_groups = {{{"format-json", "format-yaml"}, false}};

    Parser p(root);
    Argv argv = make_argv({});
    auto r = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    auto result = r.unwrap();
    // 默认值仍然生效可取。
    EXPECT_EQ(result.get("format-json"), "pretty");
    EXPECT_EQ(result.get("format-yaml"), "raw");
    EXPECT_EQ(result.source_of("format-json"), ValueSource::Default);
}

// 注入初值是显式选择：两个成员都注入仍报冲突。
TEST(OptV2ProvenanceTest, MutexInitialChoicesConflict)
{
    Arg json;
    json.name = "format-json";
    json.kind = OptKind::String;
    Arg yaml;
    yaml.name = "format-yaml";
    yaml.kind = OptKind::String;

    Command root;
    root.name         = "prog";
    root.args         = {json, yaml};
    root.mutex_groups = {{{"format-json", "format-yaml"}, false}};

    Parser p(root);
    Argv argv = make_argv({});
    auto r = p.parse(argv.argc, argv.data(),
                     {{"format-json", "pretty"}, {"format-yaml", "raw"}});
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexConflict);
}

// required 互斥组：纯默认成员不算满足，仍要求至少一个显式选择；
// 一个成员带默认、用户显式选另一个时也不报冲突。
TEST(OptV2ProvenanceTest, MutexRequiredIgnoresPureDefaults)
{
    Arg json;
    json.name          = "format-json";
    json.kind          = OptKind::String;
    json.default_value = "pretty";

    Arg xml;
    xml.name = "format-xml";
    xml.kind = OptKind::String;

    Command root;
    root.name         = "prog";
    root.args         = {json, xml};
    root.mutex_groups = {{{"format-json", "format-xml"}, true}};

    // 空命令行：默认值不算选择，报缺失。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexRequired);
    }
    // 注入初值算选择，满足 required 组。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data(), {{"format-json", "compact"}});
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().source_of("format-json"), ValueSource::Initial);
    }
    // 用户显式选择另一个带默认的成员：仅一个显式选择，无冲突。
    {
        Parser p(root);
        Argv argv = make_argv({"--format-xml", "x.xml"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_EQ(result.get("format-json"), "pretty");  // 未被选择的成员回落默认值
        EXPECT_EQ(result.source_of("format-xml"), ValueSource::CommandLine);
    }
}

// ==========================================================================
// OptionalString 可选值形态：--x 裸出现 / --x=v 内联；空格形态不消费后继
// ==========================================================================

// 裸出现 = 已提供且值为空串（stdout 约定）；内联形态取值。
TEST(OptV2OptionalTest, BareAndInlineForms)
{
    Arg dump;
    dump.name    = "dump-config-schema";
    dump.kind    = OptKind::OptionalString;
    dump.metavar = "file";

    Command root;
    root.name = "prog";
    root.args = {dump};

    // 裸出现：值为空串，来源 CommandLine。
    {
        Parser p(root);
        Argv argv = make_argv({"--dump-config-schema"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_TRUE(result.has("dump-config-schema"));
        EXPECT_EQ(result.get("dump-config-schema"), "");
        EXPECT_EQ(result.source_of("dump-config-schema"), ValueSource::CommandLine);
    }
    // 内联：写文件语义。
    {
        Parser p(root);
        Argv argv = make_argv({"--dump-config-schema=out.json"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("dump-config-schema"), "out.json");
    }
}

// 空格形态不消费后继 token：有 Positional 声明时进位置参数，
// 无声明时报 UnexpectedArgument 且指向后继 token 而非缺值。
TEST(OptV2OptionalTest, SpaceFormNeverConsumesNextToken)
{
    Arg dump;
    dump.name = "dump";
    dump.kind = OptKind::OptionalString;

    Command root;
    root.name = "prog";
    root.args = {dump};

    {
        Parser p(root);
        Argv argv = make_argv({"--dump", "out.json"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::UnexpectedArgument);
        EXPECT_EQ(r.unwrap_err().option, "out.json");
    }
    // 声明 Positional 后同样不被吞：out.json 归位置参数。
    Arg pattern;
    pattern.name = "target";
    pattern.kind = OptKind::Positional;
    root.args.push_back(pattern);
    {
        Parser p(root);
        Argv argv = make_argv({"--dump", "out.json"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_EQ(result.get("dump"), "");
        ASSERT_EQ(result.positionals().size(), 1u);
        EXPECT_EQ(result.positionals()[0], "out.json");
    }
}

// 短别名：裸 -d 与附着 -dfile；不与其它短选项组合展开。
TEST(OptV2OptionalTest, ShortBareAndAttached)
{
    Arg dump;
    dump.name    = "dump";
    dump.aliases = {"-d"};
    dump.kind    = OptKind::OptionalString;

    Command root;
    root.name = "prog";
    root.args = {dump};

    {
        Parser p(root);
        Argv argv = make_argv({"-d"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("dump"), "");
    }
    {
        Parser p(root);
        Argv argv = make_argv({"-dout.json"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("dump"), "out.json");
    }
}

// 缺席回落默认值；裸出现的空串覆盖默认值；注入初值可被 CLI 覆盖。
TEST(OptV2OptionalTest, DefaultAndInitialInterplay)
{
    Arg dump;
    dump.name          = "dump";
    dump.kind          = OptKind::OptionalString;
    dump.default_value = "schema.json";

    Command root;
    root.name = "prog";
    root.args = {dump};

    const std::unordered_map<std::string, std::string> initials = {{"dump", "from-config"}};

    // 缺席：静态默认。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_EQ(result.get("dump"), "schema.json");
        EXPECT_EQ(result.source_of("dump"), ValueSource::Default);
    }
    // 注入初值。
    {
        Parser p(root);
        Argv argv = make_argv({});
        auto r = p.parse(argv.argc, argv.data(), initials);
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("dump"), "from-config");
    }
    // CLI 裸出现：空串覆盖注入初值（stdout 约定优先于配置文件）。
    {
        Parser p(root);
        Argv argv = make_argv({"--dump"});
        auto r = p.parse(argv.argc, argv.data(), initials);
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        auto result = r.unwrap();
        EXPECT_EQ(result.get("dump"), "");
        EXPECT_EQ(result.source_of("dump"), ValueSource::CommandLine);
    }
}

// help 渲染 [=metavar] 形态；裸出现算互斥组的一次显式选择。
TEST(OptV2OptionalTest, HelpAndMutexSemantics)
{
    Arg dump;
    dump.name    = "dump-schema";
    dump.kind    = OptKind::OptionalString;
    dump.metavar = "file";

    Arg quiet;
    quiet.name = "quiet";
    quiet.kind = OptKind::Flag;

    Command root;
    root.name         = "prog";
    root.args         = {dump, quiet};
    root.mutex_groups = {{{"dump-schema", "quiet"}, false}};

    {
        Parser p(root);
        Argv argv = make_argv({"--help"});
        auto r = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_NE(r.unwrap_err().message.find("--dump-schema [=file]"), std::string::npos);
    }
    {
        Parser p(root);
        Argv argv = make_argv({"--quiet"});
        auto r = p.parse(argv.argc, argv.data(), {{"dump-schema", "a.json"}});
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::MutexConflict);
    }
}

// ==========================================================================
// v2 评审修复：子命令 usage 行、-h/--help 可覆盖、Int 空白严格化
// ==========================================================================

// 子命令层 --help：usage 行 = 程序名 + 子命令路径 + [options]，
// 不再出现旧缺陷的子命令名重复（"commit commit"）与程序名缺失。
TEST(OptV2FixTest, SubcommandHelpUsageLine)
{
    Command commit;
    commit.name = "commit";
    commit.help = "record changes";
    Arg msg;
    msg.name = "message";
    msg.kind = OptKind::String;
    msg.help = "commit message";
    commit.args.push_back(msg);

    Command root;
    root.name        = "git";
    root.help        = "git tool";
    root.subcommands = {commit};

    Parser p(root);
    Argv   argv = make_argv({"commit", "--help"});
    auto   r    = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    const auto& err = r.unwrap_err();
    EXPECT_EQ(err.category, ParseErrorCategory::HelpRequested);
    EXPECT_NE(err.message.find("Usage: git commit [options]\n"), std::string::npos);
    EXPECT_EQ(err.message.find("commit commit"), std::string::npos);
}

// 子命令自定义 usage：完整替换（含程序名，由定义方书写），
// 不再前置子命令路径（旧实现会渲染 "Usage: commit git commit ..."）。
TEST(OptV2FixTest, SubcommandCustomUsageFullyReplaces)
{
    Command commit;
    commit.name  = "commit";
    commit.help  = "record changes";
    commit.usage = "git commit --patch <file>";

    Command root;
    root.name        = "git";
    root.help        = "git tool";
    root.subcommands = {commit};

    Parser p(root);
    Argv   argv = make_argv({"commit", "--help"});
    auto   r    = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_err());
    const auto& err = r.unwrap_err();
    EXPECT_EQ(err.category, ParseErrorCategory::HelpRequested);
    EXPECT_NE(err.message.find("Usage: git commit --patch <file>\n"), std::string::npos);
}

// 根命令与 help_text 公开入口的 usage 行保持既有格式。
TEST(OptV2FixTest, RootUsageLineUnchanged)
{
    Command root;
    root.name = "mj2x";
    root.help = "convert jars";

    {
        Parser p(root);
        Argv   argv = make_argv({"--help"});
        auto   r    = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_NE(r.unwrap_err().message.find("Usage: mj2x [options]\n"), std::string::npos);
    }
    {
        const std::string help = ca::opt::help_text(root);
        EXPECT_NE(help.find("Usage: mj2x [options]\n"), std::string::npos);
    }
}

// -h 注册为 --host 的别名后按用户定义解析；未注册的 --help 仍触发内置帮助。
TEST(OptV2FixTest, ShortHelpTokenOverridable)
{
    Command root;
    root.name = "prog";
    root.help = "h as host";
    Arg host;
    host.name    = "host";
    host.kind    = OptKind::String;
    host.aliases = {"-h"};
    host.help    = "host name";
    root.args    = {host};

    {
        Parser p(root);
        Argv   argv = make_argv({"-h", "example.com"});
        auto   r    = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("host"), "example.com");
    }
    {
        Parser p(root);
        Argv   argv = make_argv({"--help"});
        auto   r    = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::HelpRequested);
    }
}

// --help 注册为普通 Flag 后按定义解析（置位而非触发帮助）。
TEST(OptV2FixTest, LongHelpTokenOverridable)
{
    Command root;
    root.name = "prog";
    root.help = "help as flag";
    Arg help_flag;
    help_flag.name    = "helper";
    help_flag.kind    = OptKind::Flag;
    help_flag.aliases = {"--help"};
    help_flag.help    = "custom help";
    root.args         = {help_flag};

    Parser p(root);
    Argv   argv = make_argv({"--help"});
    auto   r    = p.parse(argv.argc, argv.data());
    ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
    EXPECT_TRUE(r.unwrap().has("helper"));
}

// Int 严格化：前导空白（strtoll 会跳过）与尾部空白对称拒绝；显式正负号仍合法。
TEST(OptV2FixTest, IntRejectsWhitespaceForms)
{
    Command root;
    root.name = "prog";
    root.help = "x";
    Arg timeout;
    timeout.name = "timeout";
    timeout.kind = OptKind::Int;
    timeout.help = "seconds";
    root.args    = {timeout};

    for (const char* bad : {" 30", "30 ", " 30 "}) {
        Parser p(root);
        Argv   argv = make_argv({"--timeout", bad});
        auto   r    = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_err()) << "should reject: '" << bad << "'";
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::InvalidInteger);
    }
    {
        Parser p(root);
        Argv   argv = make_argv({"--timeout", "+30"});
        auto   r    = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get_int("timeout"), 30);
    }
    // 注入初值走同一严格化路径。
    {
        Parser p(root);
        Argv   argv = make_argv({});
        auto   r    = p.parse(argv.argc, argv.data(), {{"timeout", " 30"}});
        ASSERT_TRUE(r.is_err());
        EXPECT_EQ(r.unwrap_err().category, ParseErrorCategory::InvalidInteger);
    }
}

// 跨层级同名选项：父命令 CLI 显式给值不被子命令的种子（default/注入初值）覆盖
// ——「命令行恒为最高」的全局优先级跨层级成立。
TEST(OptV2FixTest, AncestorCliValueSurvivesDescendantSeed)
{
    Arg level;
    level.name = "level";
    level.kind = OptKind::String;

    Arg sub_level;
    sub_level.name          = "level";
    sub_level.kind          = OptKind::String;
    sub_level.default_value = "debug";

    Command sub;
    sub.name = "sub";
    sub.help = "sub command";
    sub.args = {sub_level};

    Command root;
    root.name        = "prog";
    root.help        = "root";
    root.args        = {level};
    root.subcommands = {sub};

    // 旧实现：进入 sub 时其 default "debug" 无条件覆盖父命令 CLI 给的 "warn"。
    {
        Parser p(root);
        Argv   argv = make_argv({"--level", "warn", "sub"});
        auto   r    = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("level"), "warn");
        EXPECT_EQ(r.unwrap().source_of("level"), ValueSource::CommandLine);
    }
    // 注入初值同样不覆盖上级 CLI 显式值。
    {
        Parser p(root);
        Argv   argv = make_argv({"--level", "warn", "sub"});
        auto   r    = p.parse(argv.argc, argv.data(), {{"level", "injected"}});
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("level"), "warn");
    }
    // 父命令未给值时，子命令种子照常生效。
    {
        Parser p(root);
        Argv   argv = make_argv({"sub"});
        auto   r    = p.parse(argv.argc, argv.data());
        ASSERT_TRUE(r.is_ok()) << r.unwrap_err().message;
        EXPECT_EQ(r.unwrap().get("level"), "debug");
        EXPECT_EQ(r.unwrap().source_of("level"), ValueSource::Default);
    }
}

}   // namespace
}   // namespace ca::opt::test
