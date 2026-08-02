#pragma once

/// @file yaml_parser.hpp
/// @brief YAML 配置子集解析器：YamlParser。把输入字节流构建为 YamlDocument。
/// @details 两层结构：先把输入切成行（记录缩进/空白/注释标记），再用 (行, 列) 游标做
///          缩进驱动的递归下降。消费 `- ` 前缀后列游标前移，行剩余部分作为"虚拟行"
///          （其缩进即当前列），统一处理 `- scalar` / `- key: v` / `- - a` 紧凑形式。
///          所有字符串经 document.arena().intern(...) 入池。
/// @note 支持范围（配置子集）：块式 mapping/sequence、YAML 1.2 core schema 标量、
///       单/双引号单行字符串、单行 flow [] {}、注释、块标量 |/>（含 -/+ chomping）、
///       开头单个 ---、BOM、CRLF。
/// @warning 明确报错拒绝：锚点 &、别名 *、标签 !、指令 %、复杂键 "? "、多文档、
///          缩进中的 TAB、重复 key、多行 plain 标量（提示改用 |/>）。首错即停。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/yaml/parse_error.hpp"
#include "libca/yaml/source_location.hpp"
#include "libca/yaml/yaml_document.hpp"
#include "libca/yaml/yaml_value.hpp"

#include <vector>

namespace ca::yaml {

/// @brief YAML 解析选项。
struct YamlParserOptions {
    /// 最大嵌套深度（块/flow 合计），超出报错以防栈溢出。
    ca::usize max_depth = 1000;
};

/// @brief YAML 配置子集解析器，把输入构建为 YamlDocument。
class YamlParser {
public:
    /// @brief 构造解析器。document 持有 arena + root；解析期间所有字符串入 arena。
    ///        输入视图须在使用期内有效（零拷贝）。
    YamlParser(YamlDocument& document, const ca::str::Utf8StringRef& input,
               const YamlParserOptions& options = YamlParserOptions());

    /// @brief 解析顶层 YAML 文档。
    /// @return 无错误返回 true；出错返回 false（首个错误可经 last_error() 取得）。
    bool run();

    /// @brief 首个解析错误。
    const ParseError& last_error() const noexcept;

private:
    /// 预扫描出的一行：不含行尾换行符，text 含前导空格。
    struct Line {
        usize offset = 0;      ///< 行首字节偏移（相对整个输入）
        usize line_no = 1;     ///< 1-based 行号
        usize indent = 0;      ///< 前导空白宽度（非空白行保证全为空格）
        const u8* text = nullptr;
        usize length = 0;
        bool blank = false;        ///< 整行只有空白
        bool comment_only = false; ///< 首个非空白字符是 '#'
    };

    // ---- 输入与状态 ----
    const u8* data_;
    usize byte_length_;
    YamlDocument& document_;
    YamlParserOptions options_;
    ParseError error_;
    bool failed_ = false;
    usize depth_ = 0;

    std::vector<Line> lines_;
    usize li_ = 0;   ///< 当前行下标
    usize col_ = 0;  ///< 行内字节列（兼作"虚拟行"缩进）

    // ---- 行预扫描 ----
    void split_lines();

    // ---- 游标 ----
    /// 前进到下一个内容位置（跳过空白行/纯注释行/行内剩余注释）。到 EOF 返回 false。
    bool advance_to_content();
    /// 当前内容的有效缩进（即列游标）。
    usize cur_indent() const noexcept { return col_; }
    /// 结束当前行：跳到下一行行首。
    void finish_line() noexcept;
    /// 当前位置的源位置。
    SourceLocation loc_here() const noexcept;
    /// 当前行 col_ 起的剩余长度。
    usize rest_length() const noexcept;

    // ---- 错误 ----
    void fail(SourceLocation loc, const char* message);

    // ---- 判定 ----
    /// 当前位置是否是序列项 '-'（后跟空白或行尾；"---" 不算）。
    bool at_sequence_dash() const noexcept;
    /// 当前位置是否是列 0 的文档分隔符 "---" / "..."。
    bool at_document_marker() const noexcept;
    /// 当前行剩余部分是否含 mapping 分隔（首个后跟空白/行尾的 ':'）。
    bool line_has_mapping_key() const noexcept;
    /// 节点首字符若是被拒绝的指示符（& * ! % @ ` 或 "? "），报错返回 true。
    bool reject_unsupported_indicator();

    // ---- 块解析 ----
    void parse_block_node(usize min_indent, YamlValue& out);
    void parse_block_sequence(usize n, YamlValue& out);
    void parse_block_mapping(usize n, YamlValue& out);
    /// mapping 条目的值：同行标量/flow/块标量，或下一行的嵌套块/零缩进序列/Null。
    void parse_mapping_value(usize n, YamlValue& out);
    /// 解析 mapping key（plain 或引号），intern 后输出；游标停在 ':' 之后的值起点。
    bool parse_mapping_key(ca::str::Utf8StringRef& out_key, SourceLocation& key_loc);

    // ---- 标量与 flow（同行） ----
    /// 同行值：引号串 / flow 集合 / plain 标量；消费到行尾（允许尾注释）。
    void parse_scalar_node(YamlValue& out);
    /// plain 标量（块上下文）：到行尾/尾注释；遇 ": " 报错。
    void parse_plain_scalar(YamlValue& out);
    /// 块标量 |/>。n 为所属上下文缩进，正文行必须缩进 > n。
    void parse_block_scalar(usize n, YamlValue& out);

    bool parse_single_quoted(ca::str::Utf8StringBuilder& out);
    bool parse_double_quoted(ca::str::Utf8StringBuilder& out);

    void parse_flow_value(YamlValue& out);
    void parse_flow_sequence(YamlValue& out);
    void parse_flow_mapping(YamlValue& out);

    /// 值后必须只剩空白/注释；随后结束本行。
    void expect_line_end();
    /// 跳过当前行 col_ 处的空格/制表符。
    void skip_inline_ws() noexcept;

    // ---- 字符串解码辅助 ----
    static bool parse_hex4(const u8* p, u32& out);
    static bool parse_hex8(const u8* p, u32& out);
};

}  // namespace ca::yaml
