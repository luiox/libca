// libca.i18n 运行时 + libca.i18n.embed-lang 规则端到端覆盖：本 target 嵌入了
// unittest/translations/{zh_CN,en_US}.lang fixture，断言经生成表查询的行为。
// 覆盖：env/--lang 语言解析、tr 回退链、trf 占位符语义、追加注册与覆盖。

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "libca/i18n/i18n.hpp"

// 规则生成头：内含 kI18nTableSet_libca_i18n_unittest（fixture 表）。
#include "i18n_libca_i18n_unittest_translations.generated.hpp"

namespace {

/// 跨平台设置环境变量（空值表示删除）。
void set_env(const char* name, const char* value)
{
#ifdef _WIN32
    std::string entry = std::string(name) + "=" + value;
    _putenv(entry.c_str());
#else
    if (value && value[0]) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}

/// 每个用例从干净状态开始：init 嵌入表，语言按 env 解析。
class I18nTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        set_env("LIBCA_I18N_TEST_LANG", "");
        ca::i18n::init({kI18nTableSet_libca_i18n_unittest}, {"LIBCA_I18N_TEST_LANG"});
        ca::i18n::set_lang("zh_CN");
    }
};

TEST_F(I18nTest, DefaultLangIsZhCn)
{
    EXPECT_STREQ(ca::i18n::lang(), "zh_CN");
    EXPECT_STREQ(ca::i18n::tr("test.hello"),
                 "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8C\xE4\xB8\x96\xE7\x95\x8C");
}

TEST_F(I18nTest, EnvSelectsLanguage)
{
    set_env("LIBCA_I18N_TEST_LANG", "en_US");
    ca::i18n::init({kI18nTableSet_libca_i18n_unittest}, {"LIBCA_I18N_TEST_LANG"});
    EXPECT_STREQ(ca::i18n::lang(), "en_US");
    EXPECT_STREQ(ca::i18n::tr("test.hello"), "Hello, world");
}

TEST_F(I18nTest, EnvTakesFirstNonEmptyRegistered)
{
    set_env("LIBCA_I18N_TEST_LANG", "fr_FR");   // 未注册：忽略
    ca::i18n::init({kI18nTableSet_libca_i18n_unittest}, {"LIBCA_I18N_TEST_LANG"});
    EXPECT_STREQ(ca::i18n::lang(), "zh_CN");
}

TEST_F(I18nTest, SetLangRejectsUnknown)
{
    EXPECT_FALSE(ca::i18n::set_lang("ja_JP"));
    EXPECT_STREQ(ca::i18n::lang(), "zh_CN");
    EXPECT_TRUE(ca::i18n::set_lang("en_US"));
    EXPECT_STREQ(ca::i18n::lang(), "en_US");
}

TEST_F(I18nTest, TrFallsBackToKeyWhenMissing)
{
    ca::i18n::set_lang("en_US");
    // en_US 有该 key 的英文文案。
    EXPECT_STREQ(ca::i18n::tr("test.hello"), "Hello, world");
    // 未命中任何语言：返回 key 原样（渐进迁移语义）。
    EXPECT_STREQ(ca::i18n::tr("test.not_exist"), "test.not_exist");
}

TEST_F(I18nTest, TrfSubstitutesPositionalArgs)
{
    ca::i18n::set_lang("zh_CN");
    EXPECT_STREQ(ca::i18n::trf("test.with_name", {"libca"}).c_str(),
                 "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8Clibca");
    // 乱序：{1} 在 {0} 之前
    EXPECT_STREQ(ca::i18n::trf("test.order", {"A", "B"}).c_str(),
                 "B \xE5\x9C\xA8 A \xE4\xB9\x8B\xE5\x89\x8D");
    // 重复占位符
    EXPECT_STREQ(ca::i18n::trf("test.repeat", {"X"}).c_str(), "X \xE4\xB8\x8E X");
}

TEST_F(I18nTest, ValueEscapesNewlineAndBackslash)
{
    // .lang 值内 \n → 换行、\\ → 反斜杠（rule 构建期反转义，多行文案依赖）。
    EXPECT_STREQ(ca::i18n::tr("test.escape"),
                 "\xE7\xAC\xAC\xE4\xB8\x80\xE8\xA1\x8C\n\xE7\xAC\xAC\xE4\xBA\x8C\xE8\xA1\x8C"
                 "\\\xE5\x8F\x8D\xE6\x96\x9C\xE6\x9D\xA0");
    ca::i18n::set_lang("en_US");
    EXPECT_STREQ(ca::i18n::tr("test.escape"), "first\nsecond\\backslash");
}

TEST_F(I18nTest, TrfKeepsMissingPlaceholdersLiteral)
{
    ca::i18n::set_lang("en_US");
    // 缺参：{1} 原样保留
    EXPECT_STREQ(ca::i18n::trf("test.missing_arg", {"only"}).c_str(),
                 "value: only and {1}");
    // 多余 args 忽略
    EXPECT_STREQ(ca::i18n::trf("test.hello", {"a", "b"}).c_str(), "Hello, world");
}

TEST_F(I18nTest, ApplyLangFromArgv)
{
    ca::i18n::set_lang("zh_CN");
    {
        char arg0[] = "prog";
        char arg1[] = "--lang";
        char arg2[] = "en_US";
        char* argv[] = {arg0, arg1, arg2};
        EXPECT_TRUE(ca::i18n::apply_lang_from_argv(3, argv));
        EXPECT_STREQ(ca::i18n::lang(), "en_US");
    }
    {
        char arg0[] = "prog";
        char arg1[] = "--lang=zh_CN";
        char* argv[] = {arg0, arg1};
        EXPECT_TRUE(ca::i18n::apply_lang_from_argv(2, argv));
        EXPECT_STREQ(ca::i18n::lang(), "zh_CN");
    }
    {
        char arg0[] = "prog";
        char arg1[] = "--lang";
        char arg2[] = "fr_FR";   // 未注册：忽略且不改语言
        char* argv[] = {arg0, arg1, arg2};
        EXPECT_FALSE(ca::i18n::apply_lang_from_argv(3, argv));
        EXPECT_STREQ(ca::i18n::lang(), "zh_CN");
    }
    {
        char arg0[] = "prog";
        char arg1[] = "-i";
        char arg2[] = "--lang=en_US";
        char* argv[] = {arg0, arg1, arg2};
        EXPECT_TRUE(ca::i18n::apply_lang_from_argv(3, argv));
        EXPECT_STREQ(ca::i18n::lang(), "en_US");
    }
}

TEST_F(I18nTest, LaterRegistrationOverridesSameKey)
{
    // 同 key 后注册者胜：模拟"产品线表覆盖共享层表"。
    static constexpr ca::i18n::Entry kOverrideEntries[] = {
        {"test.hello", "OVERRIDE"},
    };
    static constexpr ca::i18n::LangTable kOverrideTable[] = {
        {"zh_CN", kOverrideEntries, 1},
    };
    constexpr ca::i18n::TableSet kOverrideSet = {kOverrideTable, 1};
    ca::i18n::init({kOverrideSet}, {});
    EXPECT_STREQ(ca::i18n::tr("test.hello"), "OVERRIDE");
}

TEST_F(I18nTest, RegisteredLangsContainsFixture)
{
    const auto langs = ca::i18n::registered_langs();
    bool hasZh = false;
    bool hasEn = false;
    for (const char* l : langs) {
        if (std::strcmp(l, "zh_CN") == 0) hasZh = true;
        if (std::strcmp(l, "en_US") == 0) hasEn = true;
    }
    EXPECT_TRUE(hasZh);
    EXPECT_TRUE(hasEn);
}

}   // namespace
