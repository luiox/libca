#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "libca/core/status.hpp"

/// @file opt.hpp
/// @brief 命令行选项解析器，定位对标 clap/argh/argparse，保持 C++ 风格不做过度设计。
///        支持：短名(-v)/长名(--verbose)、--name value/--name=value、required/optional、
///        子命令嵌套、--help 自动生成帮助、-- 终止符。命名空间 `ca::opt`。

namespace ca::opt {

/// @brief 单个选项定义。
struct Arg
{
    /// 长名（不含 --），如 "verbose"。必须非空：它是 has()/get() 取值的唯一 key，
    /// 即便选项只用短名（如 -v）也要提供一个长名作为存储 key。
    std::string name;
    /// 短名（不含 -），如 'v'。为 0 表示无短名。
    char short_name{0};
    /// 帮助文本。
    std::string help;
    /// 是否必填。必填选项缺失时 parse 返回错误。
    /// @note required 与 default_value 互斥：有默认值意味着 has() 恒为真，
    ///       required 校验将永远不触发，build_lookup 会拒绝这种组合。
    bool required{false};
    /// 是否带值。false 表示布尔开关（--verbose），true 表示带值选项（--output FILE）。
    bool has_value{false};
    /// 带值选项的默认值；未提供时若 has_value 为真且未在命令行出现，使用此值。
    std::string default_value;
};

/// @brief 命令/子命令定义。根命令与子命令共用此结构。
struct Command
{
    /// 命令名（子命令在命令行的标识，根命令名仅用于帮助文本）。
    std::string name;
    /// 命令描述。
    std::string help;
    /// 该命令接受的选项。
    std::vector<Arg> args;
    /// 子命令列表。遇到首个非选项 token 时，按子命令名分派。
    std::vector<Command> subcommands;
};

/// @brief 解析结果。
class ParseResult
{
public:
    ParseResult() = default;

    /// @brief 指定选项是否在命令行出现。
    bool has(std::string_view name) const;

    /// @brief 取选项值。带值选项返回解析到的值（或默认值），布尔选项出现返回 "true"。
    /// @note 未出现的选项返回空串。用 has() 区分"未出现"与"出现但空值"。
    std::string get(std::string_view name) const;

    /// @brief 选中的子命令名路径（如 "git commit" 的 "commit"）。无子命令时为空。
    const std::vector<std::string>& subcommand_path() const noexcept { return subcommand_path_; }

private:
    friend class Parser;

    // 选项名 -> 出现的值（布尔开关存 "true"）。
    std::unordered_map<std::string, std::string> values_;
    std::vector<std::string>                      subcommand_path_;
};

/// @brief 命令行解析器。
class Parser
{
public:
    /// @brief 设置根命令定义（含选项与子命令树）。
    explicit Parser(Command root);

    /// @brief 解析命令行参数。
    /// @return 成功返回 ParseResult；遇非法选项/必填缺失/--help 时返回 ErrStatus。
    ///         --help 返回的 ErrStatus code 为 CANCELLED（用户请求帮助、中止正常流程），
    ///         message 为格式化的帮助文本，便于上层打印并退出；与真正的解析错误
    ///         （INVALID_ARGUMENT）区分。
    ca::core::StatusResult<ParseResult> parse(int argc, const char* const argv[]);

private:
    Command root_;
};

}  // namespace ca::opt
