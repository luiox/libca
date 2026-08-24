#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "libca/core/result.hpp"
#include "libca/core/status.hpp"

/// @file opt.hpp
/// @brief 命令行选项解析器。支持短名(-v)/长名(--verbose)、--name value/--name=value、
///        类型化选项（Flag/String/Int/StringList/OptionalString/Positional）、多别名、
///        required/default、互斥组、选项分组渲染、自定义 usage、初始值注入
///        （default < 注入初值 < 命令行）、子命令嵌套、--help 自动生成帮助、-- 终止符。
///        命名空间 `ca::opt`。
///
/// 错误以 ParseErrorCategory 类别 + 出错选项名表达，文案与 i18n 归调用方；
/// 配置文件解析与 schema dump 不进库：前者经初值注入接入，后者基于 root()
/// 元数据只读访问在下游自建。
///
/// 功能裁切（构建期裁掉选项组）通过条件注册自然实现：未注册的名字走 UnknownOption
/// 路径 fail-closed，help 由剩余选项自动生成。
///
/// 设计与取舍详见 `libca/opt/doc/opt设计文档.md`。
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
    /// 可选值字符串：值仅能内联提供（`--dump=x`）；裸出现（`--dump`）视为已提供
    /// 且值为空串（调用方自行约定空值的语义，如"输出到 stdout"）。
    /// 空格形态不消费后继 token——`--dump out.json` 中 out.json 恒为位置参数，
    /// 杜绝与位置参数/子命令的歧义。短别名支持裸形态（`-d`）与附着形态（`-dout.json`）。
    /// 典型用途：`--dump-config-schema[=file]` 不带值输出 stdout、带值写文件。
    OptionalString,
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
    /// 可选分组标签。同标签的选项在 help 中归入同一小节（标签即节标题）；
    /// 为空时归入默认 "Options:" 节。
    std::string group;
    /// 是否必填。必填选项缺失时 parse 返回错误。
    /// @note required 仅对 String/Int/StringList 生效；与 default_value 互斥：
    ///       有默认值意味着 has() 恒为真，required 校验将永远不触发。
    bool required{false};
    /// 带值选项的默认值；选项未出现且 kind 为 String/Int/OptionalString 时生效。
    std::string default_value;
};

/// @brief 互斥组：组内选项至多出现一个；required 时至少出现一个。
/// @note 「出现」按显式选择判定：只有命令行显式给出或注入初值（initial_values）
///        算作选择；静态 default_value 预置不算——两个带默认值成员同组不会误报冲突，
///        但 required 组仍要求至少一个显式选择。
struct MutexGroup
{
    /// 组内成员的 Arg::name（canonical key）列表，必须指向当前 Command 已注册的选项。
    std::vector<std::string> names;
    /// true 时整组一个都没出现报 MissingRequired 类错误。
    bool required{false};
    /// 可选组标识，仅用于错误定位（ParseError.group 回填）与下游错误文案分派；
    /// 不参与匹配与 help 渲染。为空时错误里用 "a|b" 形式的成员名拼接代替。
    std::string label;
};

/// @brief 解析错误细分类别。文案与 i18n 归调用方；message 仅提供现成英文描述。
enum class ParseErrorCategory
{
    /// 用户请求帮助（--help/-h）。message 承载完整帮助文本，arg 为空。
    HelpRequested,
    /// 未注册的选项名。
    UnknownOption,
    /// 带值选项缺值（位于参数末尾）。
    MissingValue,
    /// 带值选项收到空值（--name= 或相邻空串）。
    EmptyValue,
    /// 多余的非选项 token（未声明 Positional 也非子命令），或 Flag 选项被赋予值。
    UnexpectedArgument,
    /// required 选项 / required 互斥组缺失。
    MissingRequired,
    /// Int 选项收到非法整数或越界值。
    InvalidInteger,
    /// 互斥组内多于一个成员出现。
    MutexConflict,
    /// required 互斥组的成员一个都没出现。
    MutexRequired,
    /// 命令定义非法（如互斥组引用未注册的名字、required 与 default 并存）。
    InvalidDefinition,
};

/// @brief 解析错误：类别 + 出错选项 canonical 名 + 现成英文描述。
/// @note 字段顺序即聚合初始化顺序；group 置于末尾以兼容三元素初始化写法。
struct ParseError
{
    ParseErrorCategory category{};
    /// 出错选项 canonical 名（不含 --）。不针对单一选项时为空；
    /// UnexpectedArgument 时承载多余的裸 token。
    std::string option;
    /// 现成描述文本。HelpRequested 时为完整帮助文本；其余场景可直接打印，
    /// 也可由调用方按 category 自行格式化后丢弃。
    std::string message;
    /// 互斥类错误（MutexConflict/MutexRequired）的组标识：组有 label 时为 label，
    /// 否则为 "a|b" 形式的成员名拼接；其余类别为空。供下游按组分派文案，
    /// 避免"唯一互斥组"式硬编码。
    std::string group;
};

/// @brief 错误类别到通用状态码的桥接（供仍以 Status 为边界的调用方使用）。
///        HelpRequested -> CANCELLED；InvalidDefinition -> FAILED_PRECONDITION；
///        其余 -> INVALID_ARGUMENT。
StatusCode to_status_code(ParseErrorCategory category) noexcept;

struct Command
{
    /// 命令名（子命令在命令行的标识，根命令名仅用于帮助文本）。
    std::string name;
    /// 命令描述。
    std::string help;
    /// 该命令接受的选项。
    std::vector<Arg> args;
    /// 互斥组约束（作用于本命令的选项）。
    std::vector<MutexGroup> mutex_groups;
    /// 子命令列表。遇到非选项 token 时优先按子命令名分派。
    /// @note 选项值键（Arg::name）跨层级共享命名空间：父命令与子命令注册同名
    ///       选项时共用同一取值入口。上级命令行显式给值不会被子命令的种子
    ///       （default/注入初值）覆盖，子命令的同名显式出现按 last-wins 生效；
    ///       需要各层独立取值时避免跨层级复用同一 canonical 名。
    std::vector<Command> subcommands;
    /// 自定义 usage 行（不含 "Usage: " 前缀，如 "git [-C <path>] <command> ..."）。
    /// 为空时按命令路径（程序名 + 已穿过的子命令）与选项/位置参数/子命令自动生成；
    /// 非空时完整替换自动生成部分（含程序名与命令路径，由定义方负责书写）。
    std::string usage;
};

/// @brief 渲染一个 Command 的帮助文本（不含子命令路径前缀，程序名取 cmd.name 或
///        自定义 usage）。parse() 内部的 --help 输出与本函数共享同一实现。
/// @param groups 仅保留这些分组标签的选项节；为空时渲染全部内容。
///        位置参数与子命令摘要不受过滤影响。
std::string help_text(const Command& cmd, const std::vector<std::string>& groups = {});

/// @brief 选项值的来源。区分「命令行显式给值 / 注入初值 / 静态默认」；
///        替代下游常见的 *Selected 标志族模式（显式覆盖告警、注入条件判定等）。
enum class ValueSource
{
    /// 未提供：未出现且无默认值/注入初值。
    None,
    /// 命令行显式出现。
    CommandLine,
    /// 经 initial_values 注入（如配置文件）。
    Initial,
    /// 静态 default_value 预置。
    Default,
};

/// @brief 解析结果。
class ParseResult
{
public:
    ParseResult() = default;

    /// @brief 指定选项是否在命令行出现。
    /// @note 种子值（默认值/注入初值）也会置位本接口。需要区分来源时用 source_of()：
    ///       只有 source_of(name) == CommandLine 才代表用户在命令行显式给出。
    bool has(std::string_view name) const;

    /// @brief 查询选项值来源；未提供返回 ValueSource::None。
    ValueSource source_of(std::string_view name) const;

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
    // 选项名 -> 值来源（与 values_ 同步写入；CLI 写入覆盖种子来源）。
    std::unordered_map<std::string, ValueSource> sources_;
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

    /// @brief 根命令定义的只读访问。供下游遍历 Arg 元数据、导出 schema/补全模板；
    ///        下游自建 dump，库不提供序列化格式。
    const Command& root() const noexcept { return root_; }

    /// @brief 解析命令行参数。
    /// @return 成功返回 ParseResult；失败返回 ParseError（category + 出错选项 + 描述）。
    ///         --help/-h 返回 category = HelpRequested，message 为格式化的完整帮助文本，
    ///         便于上层打印并退出；与真正的解析错误区分。
    ///         内置 --help/-h 可被覆盖：若这两个 token 已注册为某选项的长名/别名
    ///         （如 -h 作 --host 的别名），按该选项定义解析，不再触发帮助。
    ca::core::Result<ParseResult, ParseError> parse(int argc, const char* const argv[]);

    /// @brief 解析命令行参数（带初始值注入）。
    /// @param initial_values 配置文件等外部来源的初始值，key 为选项 canonical 名。
    ///        优先级：静态 default_value < initial_values < 命令行显式出现，
    ///        即三级优先级中 CLI 恒为最高。仅对带值选项生效（Flag/Positional 忽略），
    ///        未知名字静默忽略；进入每个 command 时按该命令的选项名匹配一次。
    ///        StringList 初值按逗号拆分追加；Int 初值非法报 InvalidInteger、空串报
    ///        EmptyValue。required 把注入视为已提供；互斥组的冲突/缺失判定只把
    ///        「命令行或注入」算作选择，静态默认不算。来源可用 source_of() 查询。
    ca::core::Result<ParseResult, ParseError> parse(
        int argc, const char* const argv[],
        const std::unordered_map<std::string, std::string>& initial_values);

private:
    // 预置选项初值（静态默认 + 注入初值）。ParseResult 的友元写入点。
    static bool seed_option_values(
        const Command& cmd, const std::unordered_map<std::string, std::string>* initials,
        ParseResult& result, ParseError& err_out);

    // 命中选项后的统一取值消费（长形态 --name 与多字符单横线别名 -name 共用）：
    // Flag/OptionalString 直接收尾，带值选项取内联值（=value）或空格后继 token，
    // 按 kind 校验后以 CommandLine 来源写入 result。display 为错误文案中的 token
    // 写法（如 "--output" / "-vm-range"）。失败时写入 err_out 并返回 false；
    // 成功时 i 推进到下一个未消费参数。
    static bool consume_option_value(
        const Arg& arg, const std::string& display, bool has_inline,
        const std::string& inline_value, int argc, const char* const argv[],
        int& i, ParseResult& result, ParseError& err_out);

    Command root_;
};

}  // namespace ca::opt
