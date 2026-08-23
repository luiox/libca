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

// 该 kind 是否消费一个值。
bool kind_takes_value(OptKind kind)
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
                                                     joined)};
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
                                 ca::str::format_std("one of {} is required", joined)};
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

// kind 的 help 值占位符。
std::string kind_metavar(const Arg& arg)
{
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
    for (const auto& seg : path)
        oss << seg << ' ';
    if (!cmd.usage.empty()) {
        // 自定义 usage：完整替换自动生成部分（含程序名，由定义方负责书写）。
        oss << cmd.usage;
    }
    else {
        oss << cmd.name << " [options]";
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

// 严格十进制整数解析：全量消费、无溢出。成功写入 out。
bool parse_int_strict(const std::string& text, int& out)
{
    if (text.empty())
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

std::string help_text(const Command& cmd, const std::vector<std::string>& groups)
{
    return render_help(cmd, {}, &groups);
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

        // --help / -h：任何层级都支持。category 为 HelpRequested（用户请求帮助、中止
        // 正常流程），message 为格式化的完整帮助文本，区别于真正的解析错误。
        if (token == "--help" || token == "-h") {
            return ca::core::Err(
                ParseError{ParseErrorCategory::HelpRequested, "",
                           render_help(*current, path, nullptr)});
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
            const Arg*    arg  = it->second;
            const OptKind kind = arg->kind;

            if (!kind_takes_value(kind)) {
                if (has_inline) {
                    return ca::core::Err(ParseError{
                        ParseErrorCategory::UnexpectedArgument, arg->name,
                        ca::str::format_std("option --{} does not take a value", body)});
                }
                put_value(arg->name, "true");
                ++i;
                continue;
            }

            std::string value;
            if (has_inline) {
                value = inline_value;
            }
            else {
                if (i + 1 >= argc) {
                    return ca::core::Err(ParseError{
                        ParseErrorCategory::MissingValue, arg->name,
                        ca::str::format_std("option --{} requires a value", body)});
                }
                value = argv[++i];
            }
            if (value.empty()) {
                return ca::core::Err(ParseError{
                    ParseErrorCategory::EmptyValue, arg->name,
                    ca::str::format_std("option --{} requires a non-empty value", body)});
            }

            if (kind == OptKind::Int) {
                int parsed = 0;
                if (!parse_int_strict(value, parsed)) {
                    return ca::core::Err(ParseError{
                        ParseErrorCategory::InvalidInteger, arg->name,
                        ca::str::format_std("option --{} expects an integer, got '{}'", body,
                                            value)});
                }
                put_value(arg->name, value);  // last-wins
            }
            else if (kind == OptKind::StringList) {
                auto& dst = result.lists_[arg->name];
                for (auto& piece : split_comma(value))
                    dst.push_back(std::move(piece));  // 追加语义
                put_value(arg->name, value);
            }
            else {
                put_value(arg->name, value);  // last-wins
            }
            ++i;
            continue;
        }

        // 短选项 -x（可能带 -xvalue 或 -x value）
        if (token.size() >= 2 && token[0] == '-' && token[1] != '-') {
            char short_char = token[1];
            auto it         = lookup.by_token.find(token.substr(0, 2));
            if (it == lookup.by_token.end()) {
                return ca::core::Err(ParseError{
                    ParseErrorCategory::UnknownOption, token.substr(0, 2),
                    ca::str::format_std("unknown option: -{}", short_char)});
            }
            const Arg*    arg  = it->second;
            const OptKind kind = arg->kind;

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
