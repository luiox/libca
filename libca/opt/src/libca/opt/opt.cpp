#include "libca/opt/opt.hpp"

#include <set>
#include <sstream>
#include <utility>

namespace ca::opt {

namespace {

using ArgPtr = const Arg*;

// 为单个 command 建立长名/短名查找表。重复名视为错误（返回 nullptr 表示冲突）。
struct Lookup
{
    std::unordered_map<std::string, ArgPtr> by_long;
    std::unordered_map<char, ArgPtr>        by_short;
};

// 构建查找表；返回是否成功（名字冲突则失败，错误写入 err）。
bool build_lookup(const Command& cmd, Lookup& out, std::string& err)
{
    std::set<std::string> seen_long;
    std::set<char>        seen_short;
    for (const Arg& arg : cmd.args) {
        // name 是 has()/get() 取值的唯一 key，必须非空（短名选项也需提供长名）。
        if (arg.name.empty()) {
            err = "option with empty name is not allowed (short_name-only needs a long name as key)";
            return false;
        }
        if (arg.required && arg.has_value && !arg.default_value.empty()) {
            err = "required option --" + arg.name +
                  " cannot have a default_value (default makes required check never trigger)";
            return false;
        }
        if (!seen_long.insert(arg.name).second) {
            err = "duplicate long option: --" + arg.name;
            return false;
        }
        out.by_long[arg.name] = &arg;
        if (arg.short_name != 0) {
            if (!seen_short.insert(arg.short_name).second) {
                err = std::string("duplicate short option: -") + arg.short_name;
                return false;
            }
            out.by_short[arg.short_name] = &arg;
        }
    }
    return true;
}

// 生成单个 command 的帮助文本（含其子命令摘要）。
std::string render_help(const Command& cmd, const std::vector<std::string>& path)
{
    std::ostringstream oss;
    if (path.empty())
        oss << "Usage: " << cmd.name << " [options]";
    else {
        oss << "Usage: ";
        for (const auto& seg : path)
            oss << seg << ' ';
        oss << "[options]";
    }
    if (!cmd.subcommands.empty())
        oss << " <subcommand>";
    oss << "\n\n" << cmd.help << "\n";

    if (!cmd.args.empty()) {
        oss << "\nOptions:\n";
        for (const Arg& arg : cmd.args) {
            oss << "  ";
            if (arg.short_name != 0)
                oss << '-' << arg.short_name << ", ";
            else
                oss << "    ";
            oss << "--" << arg.name;
            if (arg.has_value)
                oss << " <value>";
            oss << "\n      " << arg.help;
            if (arg.has_value && !arg.default_value.empty())
                oss << " (default: " << arg.default_value << ")";
            if (arg.required)
                oss << " [required]";
            oss << "\n";
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

}  // namespace

bool ParseResult::has(std::string_view name) const
{
    return values_.find(std::string(name)) != values_.end();
}

std::string ParseResult::get(std::string_view name) const
{
    auto it = values_.find(std::string(name));
    if (it == values_.end())
        return std::string();
    return it->second;
}

Parser::Parser(Command root)
    : root_(std::move(root))
{}

ca::core::StatusResult<ParseResult> Parser::parse(int argc, const char* const argv[])
{
    ParseResult result;
    std::vector<std::string> path;           // 已穿过的子命令路径
    const Command*           current = &root_;

    // 预置所有带默认值的选项。
    Lookup lookup;
    std::string err;
    if (!build_lookup(*current, lookup, err))
        return ca::core::Err(ca::core::ErrStatus(ca::core::StatusCode::INVALID_ARGUMENT, err));
    for (const Arg& arg : current->args) {
        if (arg.has_value && !arg.default_value.empty())
            result.values_[arg.name] = arg.default_value;
    }

    bool only_positional = false;  // 遇到 -- 之后为真
    int  i               = 1;
    while (i < argc) {
        std::string token(argv[i]);

        if (only_positional) {
            // -- 之后全部当位置参数，目前仅用于子命令分派前的消费（这里忽略）。
            ++i;
            continue;
        }

        if (token == "--") {
            only_positional = true;
            ++i;
            continue;
        }

        // --help / -h：任何层级都支持。返回 CANCELLED（表示用户请求帮助、中止正常流程），
        // message 为格式化的帮助文本。调用方据此打印并退出，区别于真正的解析错误。
        if (token == "--help" || token == "-h") {
            return ca::core::Err(ca::core::ErrStatus(ca::core::StatusCode::CANCELLED,
                                                     render_help(*current, path)));
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

            auto it = lookup.by_long.find(body);
            if (it == lookup.by_long.end()) {
                return ca::core::Err(ca::core::ErrStatus(
                    ca::core::StatusCode::INVALID_ARGUMENT,
                    "unknown option: --" + body));
            }
            const Arg* arg = it->second;

            if (arg->has_value) {
                if (has_inline) {
                    result.values_[arg->name] = inline_value;
                }
                else {
                    if (i + 1 >= argc) {
                        return ca::core::Err(ca::core::ErrStatus(
                            ca::core::StatusCode::INVALID_ARGUMENT,
                            "option --" + body + " requires a value"));
                    }
                    result.values_[arg->name] = argv[++i];
                }
            }
            else {
                if (has_inline) {
                    return ca::core::Err(ca::core::ErrStatus(
                        ca::core::StatusCode::INVALID_ARGUMENT,
                        "option --" + body + " does not take a value"));
                }
                result.values_[arg->name] = "true";
            }
            ++i;
            continue;
        }

        // 短选项 -x（可能带 -xvalue 或 -x value）
        if (token.size() >= 2 && token[0] == '-' && token[1] != '-') {
            char short_char = token[1];
            auto it         = lookup.by_short.find(short_char);
            if (it == lookup.by_short.end()) {
                return ca::core::Err(ca::core::ErrStatus(
                    ca::core::StatusCode::INVALID_ARGUMENT,
                    std::string("unknown option: -") + short_char));
            }
            const Arg* arg = it->second;

            if (arg->has_value) {
                if (token.size() > 2) {
                    // -xvalue 形式
                    result.values_[arg->name] = token.substr(2);
                }
                else {
                    if (i + 1 >= argc) {
                        return ca::core::Err(ca::core::ErrStatus(
                            ca::core::StatusCode::INVALID_ARGUMENT,
                            std::string("option -") + short_char + " requires a value"));
                    }
                    result.values_[arg->name] = argv[++i];
                }
            }
            else {
                if (token.size() > 2) {
                    // -abc 多个短 flag 组合：逐个处理（简化：仅支持纯布尔组合）。
                    for (std::size_t c = 1; c < token.size(); ++c) {
                        char ch = token[c];
                        auto fit = lookup.by_short.find(ch);
                        if (fit == lookup.by_short.end()) {
                            return ca::core::Err(ca::core::ErrStatus(
                                ca::core::StatusCode::INVALID_ARGUMENT,
                                std::string("unknown option: -") + ch));
                        }
                        if (fit->second->has_value) {
                            return ca::core::Err(ca::core::ErrStatus(
                                ca::core::StatusCode::INVALID_ARGUMENT,
                                std::string("option -") + ch +
                                    " takes a value; cannot combine in -" + token.substr(1)));
                        }
                        result.values_[fit->second->name] = "true";
                    }
                }
                else {
                    result.values_[arg->name] = "true";
                }
            }
            ++i;
            continue;
        }

        // 非选项 token：子命令分派。
        if (const Command* sub = find_subcommand(*current, token)) {
            // 切换到子命令前，先对父命令做 required 校验：根命令的 required 不能
            // 因为进入子命令而被静默跳过。
            for (const Arg& arg : current->args) {
                if (arg.required && !result.has(arg.name)) {
                    return ca::core::Err(ca::core::ErrStatus(
                        ca::core::StatusCode::INVALID_ARGUMENT,
                        "missing required option: --" + arg.name));
                }
            }
            current = sub;
            path.push_back(sub->name);
            // 切换到子命令的选项表。
            lookup.by_long.clear();
            lookup.by_short.clear();
            if (!build_lookup(*current, lookup, err))
                return ca::core::Err(ca::core::ErrStatus(
                    ca::core::StatusCode::INVALID_ARGUMENT, err));
            for (const Arg& arg : current->args) {
                if (arg.has_value && !arg.default_value.empty())
                    result.values_[arg.name] = arg.default_value;
            }
            ++i;
            continue;
        }

        // 未知位置参数：当前实现忽略（可后续扩展收集到 positional 列表）。
        ++i;
    }

    // 检查 required 选项。
    for (const Arg& arg : current->args) {
        if (arg.required && !result.has(arg.name)) {
            return ca::core::Err(ca::core::ErrStatus(
                ca::core::StatusCode::INVALID_ARGUMENT,
                "missing required option: --" + arg.name));
        }
    }

    result.subcommand_path_ = path;
    return ca::core::Ok(std::move(result));
}

}  // namespace ca::opt
