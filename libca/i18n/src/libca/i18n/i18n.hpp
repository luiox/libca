#pragma once

// libca.i18n：嵌入式 CLI 消息国际化运行时（header-only）。
//
// 配套约定：
// - 翻译表由构建期 xmake rule(libca.i18n.embed-lang) 从 translations/*.lang
//   生成 constexpr 头嵌入二进制，运行时零文件依赖（单 exe 分发）。
// - .lang 为 UTF-8 properties 风格：key=value，`#` 注释，首个 `=` 分割，
//   值内 `{0}` `{1}` … 为占位符（可乱序/重复）。
// - 语言解析顺序：--lang 预扫描(apply_lang_from_argv) > init 传入的环境变量
//   （按序取第一个非空且已注册的）> 默认 zh_CN。
// - 查询回退：当前语言 → zh_CN → key 原样返回。回退到 key 使渐进迁移可行：
//   未进表的字面量按原样显示。
// - key 约定：小写蛇形 + 点分层，首段为命名空间（产品线/库名），
//   如 "mj2x.param.global.input.desc"。
//
// 实现为 header-only（注册表用 function-local static 保证单实例）：静态库依赖
// 本库时，其下游 binary 无需追加链接（本库自身无非模板符号；trf 返回的
// ca::str::Utf8String 依赖 libca_str，所有消费方本就链接）。
//
// 线程性：init/apply_lang_from_argv/set_lang 仅应在进程启动阶段单线程调用；
// 之后全部接口只读，可并发调用。

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"   // Utf8StringBuilder 亦定义于此

namespace ca::i18n {

/// 单条消息：key → 译文。key 须满足 ^[a-z0-9_]+(\.[a-z0-9_]+)*$（构建期校验）。
struct Entry
{
    const char* key;
    const char* val;
};

/// 一种语言的全部消息。生成端保证 entries 按 key 字典序排列，查找走二分。
struct LangTable
{
    const char*  lang;
    const Entry* entries;
    ca::usize    count;
};

/// 一次注册的一组语言表（生成头对每个 target 产出一个）。
struct TableSet
{
    const LangTable* tables;
    ca::usize        count;
};

namespace detail {

constexpr const char* kFallbackLang = "zh_CN";

/// 每种语言的合并索引：注册后按 key 字典序，重复 key 后注册者胜。
struct LangIndex
{
    std::string               lang;
    std::vector<const Entry*> entries;   // 按 key 字典序有序
};

inline std::vector<LangIndex>& lang_registry()
{
    static std::vector<LangIndex> registry;
    return registry;
}

inline std::string& current_lang()
{
    static std::string current = kFallbackLang;
    return current;
}

inline LangIndex* find_lang(const char* lang)
{
    for (auto& index : lang_registry()) {
        if (index.lang == lang)
            return &index;
    }
    return nullptr;
}

inline bool entry_key_less(const Entry* a, const Entry* b)
{
    return std::strcmp(a->key, b->key) < 0;
}

inline const Entry* lookup_in(const LangIndex& index, const char* key)
{
    auto it = std::lower_bound(
        index.entries.begin(), index.entries.end(), key, [](const Entry* e, const char* k) {
            return std::strcmp(e->key, k) < 0;
        });
    if (it == index.entries.end() || std::strcmp((*it)->key, key) != 0)
        return nullptr;
    return *it;
}

inline const char* lookup(const char* key)
{
    if (const LangIndex* index = find_lang(current_lang().c_str())) {
        if (const Entry* hit = lookup_in(*index, key))
            return hit->val;
    }
    if (std::strcmp(current_lang().c_str(), kFallbackLang) != 0) {
        if (const LangIndex* fallback = find_lang(kFallbackLang)) {
            if (const Entry* hit = lookup_in(*fallback, key))
                return hit->val;
        }
    }
    return key;
}

}   // namespace detail

/// 是否已注册过至少一组翻译表（即已调用过 init）。未初始化时 tr/trf 直接返回 key 原样。
inline bool initialized()
{
    return !detail::lang_registry().empty();
}

/// 显式切换语言；未注册的语言忽略并返回 false。
inline bool set_lang(const char* lang)
{
    if (lang == nullptr || lang[0] == '\0')
        return false;
    if (detail::find_lang(lang) == nullptr)
        return false;
    detail::current_lang() = lang;
    return true;
}

/// 注册翻译表并解析初始语言。
///
/// @param sets 全部来源的表组（如共享框架表 + 产品线表）；可多次调用追加，
///             重复 key 后注册者胜（产品线可覆盖共享层文案）。
/// @param env_names 按序取第一个非空且命中已注册语言的环境变量；全空用 zh_CN。
inline void init(std::initializer_list<TableSet> sets, std::initializer_list<const char*> env_names)
{
    auto& registry = detail::lang_registry();
    for (const TableSet& set : sets) {
        for (ca::usize t = 0; t < set.count; ++t) {
            const LangTable&   table = set.tables[t];
            detail::LangIndex* index = detail::find_lang(table.lang);
            if (index == nullptr) {
                registry.push_back(detail::LangIndex{table.lang, {}});
                index = &registry.back();
            }
            for (ca::usize e = 0; e < table.count; ++e) {
                index->entries.push_back(&table.entries[e]);
            }
        }
    }
    for (auto& index : registry) {
        std::stable_sort(index.entries.begin(), index.entries.end(), detail::entry_key_less);
        // 重复 key：后注册者胜。stable_sort 保持注册序，倒序遍历取同 key 段最后一个。
        std::vector<const Entry*> merged;
        merged.reserve(index.entries.size());
        for (auto it = index.entries.rbegin(); it != index.entries.rend(); ++it) {
            if (!merged.empty() && std::strcmp(merged.back()->key, (*it)->key) == 0) {
                continue;
            }
            merged.push_back(*it);
        }
        std::reverse(merged.begin(), merged.end());
        index.entries = std::move(merged);
    }
    // 环境变量按序取第一个非空且已注册的语言。
    for (const char* name : env_names) {
#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)   // getenv 触发 MSVC C4996 去弃用告警
#endif
        const char* value = std::getenv(name);
#if defined(_MSC_VER)
#    pragma warning(pop)
#endif
        if (value == nullptr || value[0] == '\0')
            continue;
        if (set_lang(value))
            break;
    }
}

/// 从 argv 预扫描 --lang <code> / --lang=<code> 并切换语言。
///
/// 各入口 main 应在 i18n::init 之后、正式参数解析之前调用，保证解析期错误/help
/// 已按目标语言渲染。值未注册时忽略并返回 false。
inline bool apply_lang_from_argv(int argc, char** argv)
{
    constexpr std::string_view kFlag   = "--lang";
    constexpr std::string_view kInline = "--lang=";
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        std::string_view value;
        if (arg == kFlag) {
            if (i + 1 >= argc)
                return false;
            value = argv[i + 1];
        }
        else if (arg.substr(0, kInline.size()) == kInline) {
            value = arg.substr(kInline.size());
        }
        else {
            continue;
        }
        std::string lang(value);
        if (!lang.empty() && set_lang(lang.c_str()))
            return true;
    }
    return false;
}

/// 当前语言代码；init 之前为 "zh_CN"。
inline const char* lang()
{
    return detail::current_lang().c_str();
}

/// 查询当前语言消息；回退 zh_CN；未命中返回 key 原样。
inline const char* tr(const char* key)
{
    return detail::lookup(key);
}

/// 查询并做 {0} {1} 位置参数替换（译文内可乱序/重复）。
/// 缺参的占位符原样保留；多余 args 忽略。
inline ca::str::Utf8String trf(const char* key, std::initializer_list<std::string_view> args)
{
    const char*                text = detail::lookup(key);
    ca::str::Utf8StringBuilder builder;
    const char*                literal = text;   // 当前连续字面量段起点
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p != '{')
            continue;
        // {N}：N 为十进制数字串，占位符仅支持 ASCII 数字，与 UTF-8 多字节不冲突。
        const char* q     = p + 1;
        std::size_t n     = 0;
        bool        valid = false;
        while (*q >= '0' && *q <= '9') {
            n     = n * 10 + static_cast<std::size_t>(*q - '0');
            valid = true;
            ++q;
        }
        if (valid && *q == '}' && n < args.size()) {
            builder.append(literal, static_cast<ca::usize>(p - literal));
            const std::string_view& arg = *(args.begin() + n);
            builder.append(arg.data(), static_cast<ca::usize>(arg.size()));
            p       = q;
            literal = q + 1;
        }
        // 缺参或非占位符形态：按字面量继续累积。
    }
    builder.append(literal, static_cast<ca::usize>(std::strlen(literal)));
    return builder.build_or_empty();
}

/// 已注册的全部语言代码（调试/测试用）。
inline std::vector<const char*> registered_langs()
{
    std::vector<const char*> langs;
    for (const auto& index : detail::lang_registry()) {
        langs.push_back(index.lang.c_str());
    }
    return langs;
}

}   // namespace ca::i18n
