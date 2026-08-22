#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "libca/core/status.hpp"

/// @file opt.hpp
/// @brief 命令行选项解析器。支持短名(-v)/长名(--verbose)、--name value/--name=value、
///        类型化选项（Flag/String/Int/StringList/Positional）、多别名、required/default、
///        子命令嵌套、--help 自动生成帮助、-- 终止符。命名空间 `ca::opt`。
///
/// 功能裁切（构建期裁掉选项组）通过条件注册自然实现：未注册的名字走 UnknownOption
/// 路径 fail-closed，help 由剩余选项自动生成。
namespace ca::opt {

/// @brief 选项取值类型。
enum class OptKind
{
    /// 无值开关：`-v` / `--verbose`，出现即置位。
    Flag,
    /// 单字符串值：`--output file.txt`。重复出现时后者覆盖前者（last-wins）。
    String,
    /// 整数值：`--timeout 30`。非法整数报解析错误。
    Int,
    /// 字符串列表：接受逗号拆分（`--p a,b,c`）与多次出现追加（`--p a --p b`）。
    StringList,
    /// 位置参数声明。声明后，非选项 token 收集进 ParseResult::positionals()；
    /// 未声明任何 Positional 时，多余的非选项 token 报错误。
    Positional,
};

/// @brief 单个选项定义。
struct Arg
{
    /// 长名 canonical key（不含 --），如 "verbose"。必须非空：它是 has()/get() 取值的唯一 key，
    /// 即便选项只用短名也要提供一个长名作为存储 key。
    std::string name;
    /// 附加别名 token（含前缀完整书写），如 {"-i", "--in"}。canonical 名始终是 name；
    /// 别名仅影响命令行匹配与 help 展示。
    std::vector<std::string> aliases;
    /// 取值类型。默认 String。
    OptKind kind = OptKind::String;
    /// help 中的值占位符（如 "<jar>"）。为空时按 kind 使用默认占位。
    std::string metavar;
    /// 帮助文本。
    std::string help;
    /// 是否必填。必填选项缺失时 parse 返回错误。
    /// @note required 仅对 String/Int/StringList 生效；与 default_value 互斥：
    ///       有默认值意味着 has() 恒为真，required 校验将永远不触发。
    bool required{false};
    /// 带值选项的默认值；选项未出现且 kind 为 String/Int 时生效。
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
    /// 子命令列表。遇到非选项 token 时优先按子命令名分派。
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

    /// @brief 取选项值，未出现时返回 def。
    std::string get(std::string_view name, std::string_view def) const;

    /// @brief 以整数取选项值（Int/String 选项通用）。
    /// @return 选项存在且可转换为整数时返回该值；否则返回 def。
    /// @note Int 选项的合法性已在 parse 阶段校验；本转换失败仅发生在
    ///       String 选项被当作整数读取的场景，此时安静地返回 def。
    int get_int(std::string_view name, int def = 0) const;

    /// @brief 取列表选项值（StringList）。未出现返回空列表。
    /// @note 含逗号拆分结果与多次出现追加结果，按出现顺序排列。
    std::vector<std::string> get_list(std::string_view name) const;

    /// @brief 位置参数（按命令行顺序，含 -- 之后的所有 token）。
    const std::vector<std::string>& positionals() const noexcept { return positionals_; }

    /// @brief 选中的子命令名路径（如 "git commit" 的 "commit"）。无子命令时为空。
    const std::vector<std::string>& subcommand_path() const noexcept { return subcommand_path_; }

private:
    friend class Parser;

    // 选项名 -> 出现的值（布尔开关存 "true"）。
    std::unordered_map<std::string, std::string> values_;
    // 列表选项名 -> 追加的值序列。
    std::unordered_map<std::string, std::vector<std::string>> lists_;
    std::vector<std::string> positionals_;
    std::vector<std::string> subcommand_path_;
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
