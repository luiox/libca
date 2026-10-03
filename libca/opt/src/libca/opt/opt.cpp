#include "libca/opt/opt.hpp"

#include "libca/str/format.hpp"

#include <cerrno>
#include <cstdlib>
#include <set>
#include <sstream>
#include <utility>

namespace ca::opt {

namespace {

// 单个 command 的选项匹配视图：token（含前缀的完整写法）-> Arg。
struct Lookup
{
    std::unordered_map<std::string, const Arg*> by_token;
};

// 该 kind 是否参与种子与 required 语义（有值概念）。
bool kind_takes_value(OptKind kind)
{
    return kind == OptKind::String || kind == OptKind::Int || kind == OptKind::StringList ||
           kind == OptKind::OptionalString;
}

// 该 kind 是否以空格形态消费后继 token（OptionalString 只支持内联/附着，避免歧义）。
bool consumes_space_value(OptKind kind)
{
    return kind == OptKind::String || kind == OptKind::Int || kind == OptKind::StringList;
}

// 收集一个 Arg 的全部命令行 token：长名 + 别名。别名必须以 '-' 开头。
bool collect_tokens(const Arg& arg, std::vector<std::string>& tokens, std::string& err)
{
    tokens.clear();
    tokens.push_back("--" + arg.name);
    for (const auto& alias : arg.aliases) {
        if (alias.size() < 2 || alias[0] != '-') {
            err = ca::str::format_std("alias '{}' of option --{} must start with '-'", alias,
                                      arg.name);
            return false;
        }
        tokens.push_back(alias);
    }
    return true;
}

// 构建当前 command 的 token 查找表；名字冲突或配置非法则失败（错误写入 err）。
bool build_lookup(const Command& cmd, Lookup& out, std::string& err)
{
    std::set<std::string> seen_tokens;
    for (const Arg& arg : cmd.args) {
        // name 是 has()/get() 取值的唯一 key，必须非空。
        if (arg.name.empty()) {
            err = "option with empty name is not allowed";
            return false;
        }
        if (arg.required && kind_takes_value(arg.kind) && !arg.default_value.empty()) {
            err = ca::str::format_std(
                "required option --{} cannot have a default_value (default makes required check "
                "never trigger)",
                arg.name);
            return false;
        }

        std::vector<std::string> tokens;
        if (!collect_tokens(arg, tokens, err))
            return false;
        for (const auto& tok : tokens) {
            if (!seen_tokens.insert(tok).second) {
                err = ca::str::format_std("duplicate option token: {}", tok);
                return false;
            }
            out.by_token[tok] = &arg;
        }
    }
    return true;
}

// 校验互斥组配置：成员必须指向本命令已注册的选项，且组非空。
bool validate_mutex_groups(const Command& cmd, std::string& err)
{
    for (const MutexGroup& group : cmd.mutex_groups) {
        if (group.names.empty()) {
            err = "mutex group must list at least one option name";
            return false;
        }
        for (const auto& member : group.names) {
            bool found = false;
            for (const Arg& arg : cmd.args) {
                if (arg.name == member) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                err = ca::str::format_std("mutex group references unknown option: --{}", member);
                return false;
            }
        }
    }
    return true;
}

// 互斥组的错误标识：有 label 用 label，否则用 "a|b" 形式的成员名拼接。
std::string group_identity(const MutexGroup& group)
{
    if (!group.label.empty())
        return group.label;
    std::string joined;
    for (std::size_t k = 0; k < group.names.size(); ++k) {
        if (k != 0)
            joined += "|";
        joined += group.names[k];
    }
    return joined;
}

// 运行期互斥约束检查。显式选择 = 命令行或注入初值（静态默认不算选择）：
// 多于一个选择报冲突；required 且无任何选择报缺失。
bool check_mutex_groups(const Command& cmd,
                        const std::unordered_map<std::string, ValueSource>& sources,
                        ParseError& err_out)
{
    auto chosen = [&sources](const std::string& name) {
        auto it = sources.find(name);
        return it != sources.end() &&
               (it->second == ValueSource::CommandLine || it->second == ValueSource::Initial);
    };
    for (const MutexGroup& group : cmd.mutex_groups) {
        std::vector<std::string> present;
        for (const auto& member : group.names) {
            if (chosen(member))
                present.push_back(member);
        }
        if (present.size() > 1) {
            std::string joined;
            for (std::size_t k = 0; k < present.size(); ++k) {
                if (k != 0)
                    joined += ", ";
                joined += "--" + present[k];
            }
            err_out = ParseError{ParseErrorCategory::MutexConflict, "",
                                 ca::str::format_std("mutually exclusive options given: {}",
                                                     joined),
                                 group_identity(group)};
            return false;
        }
        if (group.required && present.empty()) {
            std::string joined;
            for (std::size_t k = 0; k < group.names.size(); ++k) {
                if (k != 0)
                    joined += "|";
                joined += "--" + group.names[k];
            }
            err_out = ParseError{ParseErrorCategory::MutexRequired, "",
                                 ca::str::format_std("one of {} is required", joined),
                                 group_identity(group)};
            return false;
        }
    }
    return true;
}

// help 中选项行尾的互斥标注。
std::string mutex_suffix(const Command& cmd, const std::string& arg_name)
{
    bool in_group = false;
    bool required = false;
    for (const MutexGroup& group : cmd.mutex_groups) {
        for (const auto& member : group.names) {
            if (member == arg_name) {
                in_group = true;
                required = required || group.required;
            }
        }
    }
    if (!in_group)
        return "";
    return required ? " [exclusive, one required]" : " [exclusive]";
}

// 当前 command 是否声明了位置参数。
bool has_positional_spec(const Command& cmd)
{
    for (const Arg& arg : cmd.args) {
        if (arg.kind == OptKind::Positional)
            return true;
    }
    return false;
}

// kind 的 help 值占位符。OptionalString 用 [=...] 形态表达"值可省略"。
std::string kind_metavar(const Arg& arg)
{
    if (arg.kind == OptKind::OptionalString)
        return arg.metavar.empty() ? "[=value]" : "[=" + arg.metavar + "]";
    if (!arg.metavar.empty())
        return arg.metavar;
    switch (arg.kind) {
    case OptKind::Int:
        return "<int>";
    case OptKind::StringList:
        return "<list>";
    case OptKind::String:
        return "<value>";
    default:
        return "";
    }
}

// 生成单个 command 的帮助文本（含其子命令摘要）。
// path 为完整命令路径（含程序名与当前命令名，如 {"git", "commit"}），驱动自动
// usage 行；自定义 usage 非空时完整替换（含程序名，由定义方负责书写）。
// group_filter 非空且非空表时，仅渲染列出的分组节（未分组选项与其它分组省略）。
std::string render_help(const Command& cmd, const std::vector<std::string>& path,
                        const std::vector<std::string>* group_filter)
{
    const bool               filtering = group_filter != nullptr && !group_filter->empty();
    std::set<std::string>    filter_set;
    if (filtering)
        filter_set.insert(group_filter->begin(), group_filter->end());

    std::ostringstream oss;
    oss << "Usage: ";
    if (!cmd.usage.empty()) {
        // 自定义 usage：完整替换自动生成部分（含程序名与命令路径，由定义方负责书写）。
        oss << cmd.usage;
    }
    else {
        for (const auto& seg : path)
            oss << seg << ' ';
        oss << "[options]";
        for (const Arg& arg : cmd.args) {
            if (arg.kind == OptKind::Positional)
                oss << " <" << arg.name << ">";
        }
        if (!cmd.subcommands.empty())
            oss << " <subcommand>";
    }
    oss << "\n\n" << cmd.help << "\n";

    bool have_positional = false;
    for (const Arg& arg : cmd.args) {
        if (arg.kind == OptKind::Positional) {
            if (!have_positional) {
                oss << "\nArguments:\n";
                have_positional = true;
            }
            oss << "  <" << arg.name << ">\n      " << arg.help << "\n";
        }
    }

    // 单个选项行的渲染（默认节与分组节共用）。
    auto render_option_row = [&](const Arg& arg) {
        // 左列：全部 token 拼接，如 "-i, --input <jar>"
        std::vector<std::string> tokens;
        std::string              err;
        collect_tokens(arg, tokens, err);  // build_lookup 已验证过，此处不会失败
        oss << "  ";
        for (std::size_t k = 0; k < tokens.size(); ++k) {
            if (k != 0)
                oss << ", ";
            oss << tokens[k];
        }
        const std::string metavar = kind_metavar(arg);
        if (!metavar.empty())
            oss << ' ' << metavar;
        oss << "\n      " << arg.help;
        if (arg.kind == OptKind::StringList)
            oss << " (repeatable or comma-separated)";
        if (!metavar.empty() && !arg.default_value.empty() && arg.kind != OptKind::StringList &&
            arg.kind != OptKind::Flag)
            oss << " (default: " << arg.default_value << ")";
        if (arg.required)
            oss << " [required]";
        oss << mutex_suffix(cmd, arg.name);
        oss << "\n";
    };

    // 未分组选项进默认节；分组选项按组标签首次出现顺序各成一节。
    // 过滤模式下省略默认节，仅保留选中的分组节。
    bool have_option = false;
    for (const Arg& arg : cmd.args) {
        if (arg.kind == OptKind::Positional || !arg.group.empty())
            continue;
        if (filtering)
            continue;
        if (!have_option) {
            oss << "\nOptions:\n";
            have_option = true;
        }
        render_option_row(arg);
    }
    std::vector<std::string> group_order;
    std::set<std::string>    group_seen;
    for (const Arg& arg : cmd.args) {
        if (arg.kind == OptKind::Positional || arg.group.empty())
            continue;
        if (filtering && filter_set.find(arg.group) == filter_set.end())
            continue;
        if (group_seen.insert(arg.group).second)
            group_order.push_back(arg.group);
    }
    for (const auto& group_label : group_order) {
        oss << "\n" << group_label << ":\n";
        for (const Arg& arg : cmd.args) {
            if (arg.kind != OptKind::Positional && arg.group == group_label)
                render_option_row(arg);
        }
    }

    if (!cmd.subcommands.empty()) {
        oss << "\nSubcommands:\n";
        for (const Command& sub : cmd.subcommands)
            oss << "  " << sub.name << "\n      " << sub.help << "\n";
    }
    return oss.str();
}

// 在 cmd 的 args 中查找子命令名；返回指针或 nullptr。
const Command* find_subcommand(const Command& cmd, std::string_view token)
{
    for (const Command& sub : cmd.subcommands) {
        if (sub.name == token)
            return &sub;
    }
    return nullptr;
}

// 严格十进制整数解析：全量消费、无空白（含首尾，strtoll 跳过前导空白的行为在
// 此收紧为拒绝）、无溢出。成功写入 out。
bool parse_int_strict(const std::string& text, int& out)
{
    if (text.empty())
        return false;
    const char first = text[0];
    if (first != '+' && first != '-' && (first < '0' || first > '9'))
        return false;
    errno                 = 0;
    char*          end    = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0')
        return false;
    if (value < -2147483648LL || value > 2147483647LL)
        return false;
    out = static_cast<int>(value);
    return true;
}

// 逗号拆分（保留空段，与主流实现一致由调用方决定语义）。
std::vector<std::string> split_comma(const std::string& s)
{
    std::vector<std::string> out;
    std::size_t              start = 0;
    while (true) {
        const auto comma = s.find(',', start);
        if (comma == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, comma - start));
        start = comma + 1;
    }
    return out;
}

}  // namespace

// 预置选项初值：静态 default_value 先行，随后被 initial_values 中同名项覆盖
// （三级优先级：default < 注入初值 < 命令行）。仅带值选项参与；空串初值与非法
// 整数初值报错。失败时写入 err_out 并返回 false。
bool Parser::seed_option_values(const Command& cmd,
                                const std::unordered_map<std::string, std::string>* initials,
                                ParseResult& result, ParseError& err_out)
{
    for (const Arg& arg : cmd.args) {
        if (!kind_takes_value(arg.kind))
            continue;

        // 上级命令行已显式给值的同名选项不再被本级种子覆盖——「命令行恒为最高」
        // 的全局优先级在跨层级场景同样成立。
        auto src = result.sources_.find(arg.name);
        if (src != result.sources_.end() && src->second == ValueSource::CommandLine)
            continue;

        const std::string* init = nullptr;
        if (initials != nullptr) {
            auto it = initials->find(arg.name);
            if (it != initials->end())
                init = &it->second;
        }

        if (init != nullptr) {
            if (init->empty()) {
                err_out = ParseError{ParseErrorCategory::EmptyValue, arg.name,
                                     ca::str::format_std("initial value of --{} is empty",
                                                         arg.name)};
                return false;
            }
            result.sources_[arg.name] = ValueSource::Initial;
            if (arg.kind == OptKind::Int) {
                int parsed = 0;
                if (!parse_int_strict(*init, parsed)) {
                    err_out = ParseError{
                        ParseErrorCategory::InvalidInteger, arg.name,
                        ca::str::format_std(
                            "initial value of --{} expects an integer, got '{}'", arg.name,
                            *init)};
                    return false;
                }
                result.values_[arg.name] = *init;
            }
            else if (arg.kind == OptKind::StringList) {
                auto& dst = result.lists_[arg.name];
                for (auto& piece : split_comma(*init))
                    dst.push_back(piece);
                result.values_[arg.name] = *init;
            }
            else {
                result.values_[arg.name] = *init;
            }
            continue;
        }

        if (!arg.default_value.empty()) {
            result.values_[arg.name] = arg.default_value;
            result.sources_[arg.name] = ValueSource::Default;
        }
    }
    return true;
}

// 命中选项后的统一取值消费（--name 与多字符单横线别名 -name 共用）：
// Flag/OptionalString 直接收尾，带值选项取内联值（=value）或空格后继 token，
// 按 kind 校验（空值/整数合法性）后以 CommandLine 来源写入 result。
bool Parser::consume_option_value(
    const Arg& arg, const std::string& display, bool has_inline,
    const std::string& inline_value, int argc, const char* const argv[],
    int& i, ParseResult& result, ParseError& err_out)
{
    const OptKind kind = arg.kind;

    if (!kind_takes_value(kind)) {
        if (has_inline) {
            err_out = ParseError{
                ParseErrorCategory::UnexpectedArgument, arg.name,
                ca::str::format_std("option {} does not take a value", display)};
            return false;
        }
        result.values_[arg.name]  = "true";
        result.sources_[arg.name] = ValueSource::CommandLine;
        ++i;
        return true;
    }

    if (kind == OptKind::OptionalString) {
        // 值仅内联提供；裸出现 = 已提供且值为空串（调用方约定语义，如 stdout），
        // 不消费后继 token，杜绝与位置参数/子命令的歧义。
        result.values_[arg.name]  = has_inline ? inline_value : std::string{};
        result.sources_[arg.name] = ValueSource::CommandLine;
        ++i;
        return true;
    }

    std::string value;
    if (has_inline) {
        value = inline_value;
    }
    else {
        if (i + 1 >= argc) {
            err_out = ParseError{
                ParseErrorCategory::MissingValue, arg.name,
                ca::str::format_std("option {} requires a value", display)};
            return false;
        }
        value = argv[++i];
    }
    if (value.empty()) {
        err_out = ParseError{
            ParseErrorCategory::EmptyValue, arg.name,
            ca::str::format_std("option {} requires a non-empty value", display)};
        return false;
    }

    if (kind == OptKind::Int) {
        int parsed = 0;
        if (!parse_int_strict(value, parsed)) {
            err_out = ParseError{
                ParseErrorCategory::InvalidInteger, arg.name,
                ca::str::format_std("option {} expects an integer, got '{}'", display, value)};
            return false;
        }
    }
    else if (kind == OptKind::StringList) {
        auto& dst = result.lists_[arg.name];
        for (auto& piece : split_comma(value))
            dst.push_back(std::move(piece));  // 追加语义
    }
    result.values_[arg.name]  = value;  // last-wins
    result.sources_[arg.name] = ValueSource::CommandLine;
    ++i;
    return true;
}

std::string help_text(const Command& cmd, const std::vector<std::string>& groups)
{
    // 公开入口不带子命令路径前缀：path 只含 cmd.name 自身。
    return render_help(cmd, {cmd.name}, &groups);
}

// ---- HelpTable: help 行排版表（issue luiox/morpher#890）----

namespace {

// UTF-8 codepoint count: every non-continuation byte ((c & 0xC0) != 0x80)
// starts one codepoint. （Codepoint 口径的历史实现，保持逐字节行为不变。）
std::size_t help_utf8_codepoint_count(std::string_view text) noexcept
{
    std::size_t count = 0;
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

// UTF-8 解码：从 s[i] 起解码一个码点。合法序列写 cp 并推进 i 返回 true；
// 非法字节（含截断序列）消费 1 字节、cp 置替换符返回 false（该字节按宽度 1
// 计入，内容不丢）。
bool help_utf8_next(std::string_view s, std::size_t& i, char32_t& cp) noexcept
{
    const auto lead = static_cast<unsigned char>(s[i]);
    std::size_t len   = 0;
    char32_t    value = 0;
    if (lead < 0x80) {
        cp = lead;
        i += 1;
        return true;
    }
    if ((lead & 0xE0) == 0xC0) {
        len   = 2;
        value = lead & 0x1Fu;
    }
    else if ((lead & 0xF0) == 0xE0) {
        len   = 3;
        value = lead & 0x0Fu;
    }
    else if ((lead & 0xF8) == 0xF0) {
        len   = 4;
        value = lead & 0x07u;
    }
    else {
        cp = 0xFFFD;
        i += 1;
        return false;
    }
    if (i + len > s.size()) {
        cp = 0xFFFD;
        i += 1;
        return false;
    }
    for (std::size_t k = 1; k < len; ++k) {
        const auto cont = static_cast<unsigned char>(s[i + k]);
        if ((cont & 0xC0) != 0x80) {
            cp = 0xFFFD;
            i += 1;
            return false;
        }
        value = (value << 6) | (cont & 0x3Fu);
    }
    cp = value;
    i += len;
    return true;
}

// 单码点终端显示宽度近似（East Asian Width 精简表）：Wide/Fullwidth 计 2，
// 组合符号/零宽字符/控制字符计 0，其余计 1。 help 排版足够，不做完整 EAW 表。
std::size_t help_codepoint_display_width(char32_t cp) noexcept
{
    if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return 0;   // 控制字符
    if ((cp >= 0x0300 && cp <= 0x036F) ||                    // 组合附加符号
        (cp >= 0x200B && cp <= 0x200F) ||                    // 零宽字符与方向标记
        (cp >= 0xFE00 && cp <= 0xFE0F) ||                    // 变体选择符
        cp == 0xFEFF)                                        // 零宽不换行空格
        return 0;
    const bool wide =
        (cp >= 0x1100 && cp <= 0x115F) ||   // Hangul Jamo
        (cp >= 0x2E80 && cp <= 0x303E) ||   // CJK 部首与符号
        (cp >= 0x3041 && cp <= 0x33FF) ||   // 假名、注音、CJK 兼容方块
        (cp >= 0x3400 && cp <= 0x4DBF) ||   // CJK 扩展 A
        (cp >= 0x4E00 && cp <= 0x9FFF) ||   // CJK 统一表意文字
        (cp >= 0xA000 && cp <= 0xA4CF) ||   // 彝文
        (cp >= 0xA960 && cp <= 0xA97F) ||   // Hangul Jamo 扩展 A
        (cp >= 0xAC00 && cp <= 0xD7A3) ||   // Hangul 音节
        (cp >= 0xF900 && cp <= 0xFAFF) ||   // CJK 兼容表意文字
        (cp >= 0xFE10 && cp <= 0xFE19) ||   // 竖排形式
        (cp >= 0xFE30 && cp <= 0xFE6F) ||   // CJK 兼容形式
        (cp >= 0xFF00 && cp <= 0xFF60) ||   // 全角 ASCII 与标点
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||   // 全角符号
        (cp >= 0x1F300 && cp <= 0x1F64F) || // emoji 常用块
        (cp >= 0x1F680 && cp <= 0x1F6FF) ||
        (cp >= 0x1F900 && cp <= 0x1F9FF) ||
        (cp >= 0x20000 && cp <= 0x2FFFD) || // CJK 扩展 B 起
        (cp >= 0x30000 && cp <= 0x3FFFD);
    return wide ? 2 : 1;
}

// 按计量口径测字符串宽度：Codepoint = 码点数（历史口径）；Display = 显示宽度。
std::size_t help_string_width(std::string_view text, WidthMode mode) noexcept
{
    if (mode == WidthMode::Codepoint) return help_utf8_codepoint_count(text);
    std::size_t width = 0;
    std::size_t i     = 0;
    char32_t    cp    = 0;
    while (i < text.size()) {
        help_utf8_next(text, i, cp);
        width += help_codepoint_display_width(cp);
    }
    return width;
}

// 折行 token：一段不可再断的文本——空格段、普通词（非宽字符连续段）或单个
// 宽字符（宽字符前后皆可断行，CJK 无空格分词）。
struct HelpWrapToken
{
    std::string text;
    std::size_t width;
    bool        is_space;
};

// 把一行按断点切成 token（宽度按 mode 口径）。
std::vector<HelpWrapToken> help_wrap_tokenize(std::string_view line, WidthMode mode)
{
    std::vector<HelpWrapToken> tokens;
    std::size_t                i = 0;
    while (i < line.size()) {
        const std::size_t space_start = i;
        while (i < line.size() && line[i] == ' ') ++i;
        if (i > space_start) {
            const std::string text{line.substr(space_start, i - space_start)};
            tokens.push_back(HelpWrapToken{text, help_string_width(text, mode), true});
            continue;
        }
        std::string cur;
        std::size_t cur_w = 0;
        while (i < line.size() && line[i] != ' ') {
            const std::size_t begin = i;
            char32_t          cp    = 0;
            help_utf8_next(line, i, cp);
            const std::size_t w = help_codepoint_display_width(cp);
            if (w == 2) {
                // 宽字符自成断点段：先落此前积累的普通词。
                if (!cur.empty()) {
                    tokens.push_back(HelpWrapToken{cur, cur_w, false});
                    cur.clear();
                    cur_w = 0;
                }
                tokens.push_back(HelpWrapToken{std::string(line.substr(begin, i - begin)), w, false});
            }
            else {
                cur.append(line.substr(begin, i - begin));
                cur_w += w;
            }
        }
        if (!cur.empty()) tokens.push_back(HelpWrapToken{cur, cur_w, false});
    }
    return tokens;
}

// 贪心折行：avail = 本段可用宽度（0 = 不折行，整行原样返回）。断点处行尾空格
// 丢弃；单词超过整行可用宽度时按宽度硬切，永不截断丢字。
std::vector<std::string> help_wrap_line(std::string_view line, std::size_t avail, WidthMode mode)
{
    if (avail == 0 || help_string_width(line, mode) <= avail)
        return {std::string(line)};
    std::vector<std::string> out;
    std::string              cur;
    std::size_t              cur_w = 0;
    const auto               flush = [&] {
        // 断点空格已先行挂上 cur，落行时剥掉：折行产生的行尾不留空白
        // （对齐口径；与未折行行的 verbatim 原样区分）。
        while (!cur.empty() && cur.back() == ' ') cur.pop_back();
        out.push_back(cur);
        cur.clear();
        cur_w = 0;
    };
    for (const HelpWrapToken& tok : help_wrap_tokenize(line, mode)) {
        if (tok.is_space) {
            if (cur_w + tok.width <= avail) {
                cur += tok.text;
                cur_w += tok.width;
            }
            else if (cur_w > 0) {
                flush();   // 断点：行尾空格丢弃；行首空格超宽同样丢弃
            }
            continue;
        }
        if (tok.width > avail) {
            if (cur_w > 0) flush();
            // 硬切：逐字符按宽度累计，满 avail 即断段。
            std::size_t i = 0, piece_w = 0, piece_begin = 0;
            while (i < tok.text.size()) {
                const std::size_t ch_begin = i;
                char32_t          cp       = 0;
                help_utf8_next(tok.text, i, cp);
                const std::size_t w =
                    mode == WidthMode::Display ? help_codepoint_display_width(cp) : 1;
                if (piece_w + w > avail && piece_w > 0) {
                    out.push_back(tok.text.substr(piece_begin, ch_begin - piece_begin));
                    piece_begin = ch_begin;
                    piece_w     = 0;
                }
                piece_w += w;
            }
            cur   = tok.text.substr(piece_begin);
            cur_w = piece_w;
            continue;
        }
        if (cur_w + tok.width > avail && cur_w > 0) flush();
        cur += tok.text;
        cur_w += tok.width;
    }
    if (!cur.empty() || out.empty()) flush();
    return out;
}

// 单元格展开为物理行：'\n' 为硬换行边界（续行由渲染层悬挂对齐）；avail > 0 时
// 每段硬行再按宽度折行。
std::vector<std::string> help_cell_lines(std::string_view cell, std::size_t avail, WidthMode mode)
{
    std::vector<std::string> lines;
    std::size_t              start = 0;
    while (true) {
        const std::size_t nl = cell.find('\n', start);
        const std::string_view hard = nl == std::string_view::npos
                                          ? cell.substr(start)
                                          : cell.substr(start, nl - start);
        for (std::string& piece : help_wrap_line(hard, avail, mode))
            lines.push_back(std::move(piece));
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return lines;
}

// 折行可用宽度：折行开启且列起点在总宽内时返回剩余宽度，否则 0（不折行）。
std::size_t help_wrap_avail(std::size_t text_width, std::size_t column_start) noexcept
{
    return column_start < text_width ? text_width - column_start : 0;
}

}   // namespace

void HelpTable::add(std::string name, std::string description)
{
    std::vector<std::string> cells;
    cells.reserve(2);
    cells.push_back(std::move(name));
    cells.push_back(std::move(description));
    entries_.push_back(Entry{false, std::move(cells)});
}

void HelpTable::add_row(std::vector<std::string> cells)
{
    if (cells.empty()) cells.push_back(std::string());   // 空行：仍按一行结构行参与渲染
    entries_.push_back(Entry{false, std::move(cells)});
}

void HelpTable::add_raw(std::string line)
{
    entries_.push_back(Entry{true, {std::move(line)}});
}

void HelpTable::append(const HelpTable& other)
{
    for (const Entry& entry : other.entries_) entries_.push_back(entry);
}

std::string HelpTable::render(const std::size_t indent, const std::size_t gap) const
{
    // 描述列 = indent + 最长名宽 + gap；名宽在渲染期按当前口径全量重算，
    // 原样行不参与度量。
    std::size_t max_name_width = 0;
    for (const Entry& entry : entries_) {
        if (entry.raw || entry.cells.empty()) continue;
        const std::size_t width = help_string_width(entry.cells[0], width_mode_);
        if (width > max_name_width) max_name_width = width;
    }
    return render_to_column(indent, indent + max_name_width + gap);
}

std::string HelpTable::render_to_column(const std::size_t indent,
                                        const std::size_t description_column) const
{
    std::string out;
    const std::size_t avail = help_wrap_avail(text_width_, description_column);
    for (const Entry& entry : entries_) {
        if (entry.raw) {
            out += entry.cells[0];
            out.push_back('\n');
            continue;
        }
        const std::string& name = entry.cells[0];
        // 两列口径：第一格为名，其余格以单空格连接作描述。
        std::string description;
        for (std::size_t c = 1; c < entry.cells.size(); ++c) {
            if (c > 1) description.push_back(' ');
            description += entry.cells[c];
        }
        if (description.empty()) {
            out.append(indent, ' ');
            out += name;
            out.push_back('\n');
            continue;
        }
        const std::size_t name_width = help_string_width(name, width_mode_);
        const std::size_t used       = indent + name_width;
        const std::size_t pad        = description_column > used ? description_column - used : 1;
        const std::vector<std::string> desc_lines = help_cell_lines(description, avail, width_mode_);
        out.append(indent, ' ');
        out += name;
        out.append(pad, ' ');
        out += desc_lines[0];
        out.push_back('\n');
        for (std::size_t k = 1; k < desc_lines.size(); ++k) {
            std::string line;
            line.append(description_column, ' ');  // 续行悬挂对齐到描述列
            line += desc_lines[k];
            // 对齐口径与 render_columns 一致：行尾不留空白（空硬续行不产尾随空格）。
            while (!line.empty() && line.back() == ' ') line.pop_back();
            out += line;
            out.push_back('\n');
        }
    }
    return out;
}

std::string HelpTable::render_columns(const std::size_t indent, const std::size_t gap) const
{
    // 列数 = 结构行最大格数；各列宽 = 该列最大格宽（渲染期按当前口径计算）。
    std::size_t columns = 0;
    for (const Entry& entry : entries_) {
        if (!entry.raw) columns = columns > entry.cells.size() ? columns : entry.cells.size();
    }
    std::string out;
    if (columns == 0) {
        // 纯原样行表：直接拼接。
        for (const Entry& entry : entries_) {
            out += entry.cells[0];
            out.push_back('\n');
        }
        return out;
    }
    std::vector<std::size_t> col_width(columns, 0);
    for (const Entry& entry : entries_) {
        if (entry.raw) continue;
        for (std::size_t c = 0; c < entry.cells.size(); ++c) {
            const std::size_t width = help_string_width(entry.cells[c], width_mode_);
            if (width > col_width[c]) col_width[c] = width;
        }
    }
    // 末列起始列 = indent + 前列宽和 + 列间距和；折行只作用于末列。
    std::size_t last_start = indent;
    for (std::size_t c = 0; c + 1 < columns; ++c) last_start += col_width[c] + gap;
    const std::size_t last_avail = help_wrap_avail(text_width_, last_start);

    static const std::string k_empty;
    for (const Entry& entry : entries_) {
        if (entry.raw) {
            out += entry.cells[0];
            out.push_back('\n');
            continue;
        }
        // 各格展开物理行：仅落在表末列上的格折行，其余格只展开 '\n' 硬换行。
        std::vector<std::vector<std::string>> col_lines(entry.cells.size());
        std::size_t                           line_count = 0;
        for (std::size_t c = 0; c < entry.cells.size(); ++c) {
            const std::size_t avail = c + 1 == columns ? last_avail : 0;
            col_lines[c]            = help_cell_lines(entry.cells[c], avail, width_mode_);
            if (col_lines[c].size() > line_count) line_count = col_lines[c].size();
        }
        for (std::size_t li = 0; li < line_count; ++li) {
            std::string line;
            line.append(indent, ' ');
            for (std::size_t c = 0; c < columns; ++c) {
                const bool has_text = c < col_lines.size() && li < col_lines[c].size();
                const std::string& text = has_text ? col_lines[c][li] : k_empty;
                line += text;
                if (c + 1 < columns) {
                    // 补齐本列宽 + 列间距；续行超宽时 pad 为 0（右推后续列，不截断）。
                    const std::size_t text_w = help_string_width(text, width_mode_);
                    const std::size_t pad =
                        col_width[c] > text_w ? col_width[c] - text_w : 0;
                    line.append(pad + gap, ' ');
                }
            }
            // 对齐口径：结构行行尾不留空白（空末格、缺格续行同样干净）。
            while (!line.empty() && line.back() == ' ') line.pop_back();
            out += line;
            out.push_back('\n');
        }
    }
    return out;
}


StatusCode to_status_code(ParseErrorCategory category) noexcept
{
    switch (category) {
    case ParseErrorCategory::HelpRequested:
        return StatusCode::CANCELLED;
    case ParseErrorCategory::InvalidDefinition:
        return StatusCode::FAILED_PRECONDITION;
    default:
        return StatusCode::INVALID_ARGUMENT;
    }
}

bool ParseResult::has(std::string_view name) const
{
    return values_.find(std::string(name)) != values_.end();
}

ValueSource ParseResult::source_of(std::string_view name) const
{
    auto it = sources_.find(std::string(name));
    if (it == sources_.end())
        return ValueSource::None;
    return it->second;
}

std::string ParseResult::get(std::string_view name) const
{
    auto it = values_.find(std::string(name));
    if (it == values_.end())
        return std::string();
    return it->second;
}

std::string ParseResult::get(std::string_view name, std::string_view def) const
{
    auto it = values_.find(std::string(name));
    if (it == values_.end())
        return std::string(def);
    return it->second;
}

int ParseResult::get_int(std::string_view name, int def) const
{
    auto it = values_.find(std::string(name));
    if (it == values_.end())
        return def;
    int value = 0;
    if (!parse_int_strict(it->second, value))
        return def;
    return value;
}

std::vector<std::string> ParseResult::get_list(std::string_view name) const
{
    auto it = lists_.find(std::string(name));
    if (it == lists_.end())
        return std::vector<std::string>();
    return it->second;
}

Parser::Parser(Command root)
    : root_(std::move(root))
{}

ca::core::Result<ParseResult, ParseError> Parser::parse(int argc, const char* const argv[])
{
    return parse(argc, argv, {});
}

ca::core::Result<ParseResult, ParseError> Parser::parse(
    int argc, const char* const argv[],
    const std::unordered_map<std::string, std::string>& initial_values)
{
    ParseResult result;
    std::vector<std::string> path;           // 已穿过的子命令路径
    const Command*           current = &root_;

    // 命令行显式写入：值与来源同步登记。
    auto put_value = [&result](const std::string& name, const std::string& v) {
        result.values_[name]   = v;
        result.sources_[name]  = ValueSource::CommandLine;
    };

    // 预置选项初值（静态默认 + 注入初值）。
    Lookup lookup;
    std::string err;
    if (!build_lookup(*current, lookup, err))
        return ca::core::Err(ParseError{ParseErrorCategory::InvalidDefinition, "", err});
    if (!validate_mutex_groups(*current, err))
        return ca::core::Err(ParseError{ParseErrorCategory::InvalidDefinition, "", err});
    {
        ParseError seed_err;
        if (!seed_option_values(*current, &initial_values, result, seed_err))
            return ca::core::Err(std::move(seed_err));
    }

    bool only_positional = false;  // 遇到 -- 之后为真
    int  i               = 1;
    while (i < argc) {
        const std::string token(argv[i]);

        if (only_positional) {
            // -- 之后全部按位置参数收集。
            result.positionals_.push_back(token);
            ++i;
            continue;
        }

        if (token == "--") {
            only_positional = true;
            ++i;
            continue;
        }

        // --help / -h：任何层级都支持，但可被用户注册的同名选项覆盖（如 -h 作
        // --host 的别名）——注册了的 token 优先按用户定义解析。category 为
        // HelpRequested（用户请求帮助、中止正常流程），message 为格式化的完整
        // 帮助文本，区别于真正的解析错误。
        const bool builtin_help = (token == "--help" || token == "-h") &&
                                  lookup.by_token.find(token) == lookup.by_token.end();
        if (builtin_help) {
            // usage 行的命令路径：程序名 + 已穿过的子命令（当前命令含在其中）。
            std::vector<std::string> help_path;
            help_path.push_back(root_.name);
            help_path.insert(help_path.end(), path.begin(), path.end());
            return ca::core::Err(ParseError{
                ParseErrorCategory::HelpRequested, "", render_help(*current, help_path, nullptr)});
        }

        // 长选项 --name 或 --name=value
        if (token.size() >= 2 && token[0] == '-' && token[1] == '-') {
            std::string body = token.substr(2);
            std::string inline_value;
            bool        has_inline = false;
            auto        eq = body.find('=');
            if (eq != std::string::npos) {
                inline_value = body.substr(eq + 1);
                body         = body.substr(0, eq);
                has_inline   = true;
            }

            auto it = lookup.by_token.find("--" + body);
            if (it == lookup.by_token.end()) {
                return ca::core::Err(ParseError{
                    ParseErrorCategory::UnknownOption, body,
                    ca::str::format_std("unknown option: --{}", body)});
            }
            {
                ParseError err;
                if (!consume_option_value(*it->second, "--" + body, has_inline,
                                          inline_value, argc, argv, i, result, err))
                    return ca::core::Err(std::move(err));
            }
            continue;
        }

        // 短选项 -x（可能带 -xvalue 或 -x value）
        if (token.size() >= 2 && token[0] == '-' && token[1] != '-') {
            // 多字符单横线别名（如 -vm-range）：先按整 token（剥离 =value 后）
            // 精确匹配，命中则与长形态同语义取值；不进入短旗标簇与 -xvalue
            // 附着展开（簇与附着形态仅对两字符短名定义）。
            if (token.size() > 2) {
                std::string probe        = token;
                std::string inline_value;
                bool        has_inline   = false;
                const auto  eq           = probe.find('=');
                if (eq != std::string::npos) {
                    inline_value = probe.substr(eq + 1);
                    probe        = probe.substr(0, eq);
                    has_inline   = true;
                }
                if (probe.size() > 2) {
                    auto full = lookup.by_token.find(probe);
                    if (full != lookup.by_token.end()) {
                        ParseError err;
                        if (!consume_option_value(*full->second, probe, has_inline,
                                                  inline_value, argc, argv, i, result, err))
                            return ca::core::Err(std::move(err));
                        continue;
                    }
                    if (has_inline) {
                        // -name=value 但 name 未注册：按未知选项报全名，
                        // 不落入短簇拆分产生截断的错误定位。
                        return ca::core::Err(ParseError{
                            ParseErrorCategory::UnknownOption, probe,
                            ca::str::format_std("unknown option: {}", probe)});
                    }
                }
            }

            char short_char = token[1];
            auto it         = lookup.by_token.find(token.substr(0, 2));
            if (it == lookup.by_token.end()) {
                return ca::core::Err(ParseError{
                    ParseErrorCategory::UnknownOption, token,
                    ca::str::format_std("unknown option: {}", token)});
            }
            const Arg*    arg  = it->second;
            const OptKind kind = arg->kind;

            if (kind == OptKind::OptionalString) {
                // 裸（-d）或附着（-dout.json）形态；不消费后继 token、不参与组合展开。
                put_value(arg->name, token.size() > 2 ? token.substr(2) : std::string{});
                ++i;
                continue;
            }

            if (!kind_takes_value(kind)) {
                if (token.size() > 2) {
                    // -abc 多个短 flag 组合：逐个处理（仅支持纯布尔组合）。
                    for (std::size_t c = 1; c < token.size(); ++c) {
                        const char ch  = token[c];
                        auto       fit = lookup.by_token.find(std::string("-") + ch);
                        if (fit == lookup.by_token.end()) {
                            return ca::core::Err(ParseError{
                                ParseErrorCategory::UnknownOption, std::string("-") + ch,
                                ca::str::format_std("unknown option: -{}", ch)});
                        }
                        if (kind_takes_value(fit->second->kind)) {
                            return ca::core::Err(ParseError{
                                ParseErrorCategory::UnexpectedArgument, fit->second->name,
                                ca::str::format_std(
                                    "option -{} takes a value; cannot combine in -{}", ch,
                                    token.substr(1))});
                        }
                        put_value(fit->second->name, "true");
                    }
                }
                else {
                    put_value(arg->name, "true");
                }
                ++i;
                continue;
            }

            std::string value;
            if (token.size() > 2) {
                value = token.substr(2);  // -xvalue 形式
            }
            else {
                if (i + 1 >= argc) {
                    return ca::core::Err(ParseError{
                        ParseErrorCategory::MissingValue, arg->name,
                        ca::str::format_std("option -{} requires a value", short_char)});
                }
                value = argv[++i];
            }
            if (value.empty()) {
                return ca::core::Err(ParseError{
                    ParseErrorCategory::EmptyValue, arg->name,
                    ca::str::format_std("option -{} requires a non-empty value", short_char)});
            }

            if (kind == OptKind::Int) {
                int parsed = 0;
                if (!parse_int_strict(value, parsed)) {
                    return ca::core::Err(ParseError{
                        ParseErrorCategory::InvalidInteger, arg->name,
                        ca::str::format_std("option -{} expects an integer, got '{}'",
                                            short_char, value)});
                }
                put_value(arg->name, value);
            }
            else if (kind == OptKind::StringList) {
                auto& dst = result.lists_[arg->name];
                for (auto& piece : split_comma(value))
                    dst.push_back(std::move(piece));
                put_value(arg->name, value);
            }
            else {
                put_value(arg->name, value);
            }
            ++i;
            continue;
        }

        // 非选项 token：优先子命令分派，其次位置参数收集。
        if (const Command* sub = find_subcommand(*current, token)) {
            // 切换到子命令前，先对父命令收尾校验（required 与互斥组）：根命令的约束不能
            // 因为进入子命令而被静默跳过。
            ParseError finalize_err;
            bool       failed = false;
            for (const Arg& arg : current->args) {
                if (arg.required && kind_takes_value(arg.kind) && !result.has(arg.name)) {
                    finalize_err = ParseError{
                        ParseErrorCategory::MissingRequired, arg.name,
                        ca::str::format_std("missing required option: --{}", arg.name)};
                    failed = true;
                    break;
                }
            }
            if (!failed && !check_mutex_groups(*current, result.sources_, finalize_err))
                failed = true;
            if (failed)
                return ca::core::Err(std::move(finalize_err));

            current = sub;
            path.push_back(sub->name);
            // 切换到子命令的选项表与互斥组配置，并预置其初值。
            lookup.by_token.clear();
            if (!build_lookup(*current, lookup, err))
                return ca::core::Err(ParseError{ParseErrorCategory::InvalidDefinition, "", err});
            if (!validate_mutex_groups(*current, err))
                return ca::core::Err(ParseError{ParseErrorCategory::InvalidDefinition, "", err});
            {
                ParseError seed_err;
                if (!seed_option_values(*current, &initial_values, result, seed_err))
                    return ca::core::Err(std::move(seed_err));
            }
            ++i;
            continue;
        }

        if (has_positional_spec(*current)) {
            result.positionals_.push_back(token);
            ++i;
            continue;
        }

        // 无 Positional 声明时的多余裸 token：报错而非静默丢弃（v2 行为变更）。
        return ca::core::Err(ParseError{
            ParseErrorCategory::UnexpectedArgument, token,
            ca::str::format_std("unexpected argument: {}", token)});
    }

    // 收尾校验当前命令：required 选项与互斥组。
    for (const Arg& arg : current->args) {
        if (arg.required && kind_takes_value(arg.kind) && !result.has(arg.name)) {
            return ca::core::Err(ParseError{
                ParseErrorCategory::MissingRequired, arg.name,
                ca::str::format_std("missing required option: --{}", arg.name)});
        }
    }
    ParseError mutex_err;
    if (!check_mutex_groups(*current, result.sources_, mutex_err))
        return ca::core::Err(std::move(mutex_err));

    result.subcommand_path_ = path;
    return ca::core::Ok(std::move(result));
}

}  // namespace ca::opt
