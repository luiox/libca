#pragma once

/// @file toml_parser.hpp
/// @brief TOML 递归下降解析器：TomlParser。把输入字节流构建为 TomlDocument。
/// @details TOML 是面向行的格式：每行要么是 table header `[a.b]` / `[[a.b]]`，要么是
///          key = value 行，要么是空行/注释。解析器单趟扫描，按行识别后驱动字面量解析和
///          table 组织。所有字符串经 document.arena().intern(...) 入池。
/// @note TOML 1.0 允许 UTF-8 BOM。错误首错即停。
/// @warning 重复 key / 重复 table header / inline table 不可变约束违反 / 整数溢出 → 全部报错。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "libca/toml/parse_error.hpp"
#include "libca/toml/source_location.hpp"
#include "libca/toml/toml_datetime.hpp"
#include "libca/toml/toml_document.hpp"
#include "libca/toml/toml_value.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace ca::toml {

/// @brief TOML 解析选项（TOML 1.0 严格模式，目前无可配项，保留扩展位）。
struct TomlParserOptions {
    /// 最大嵌套深度（数组/表/inline table），超出报错以防栈溢出。
    ca::usize max_depth = 1000;
};

/// @brief TOML 递归下降解析器，把输入构建为 TomlDocument。
class TomlParser {
public:
    /// @brief 构造解析器。document 持有 arena + root；解析期间所有字符串入 arena。
    ///        输入视图须在使用期内有效（零拷贝）。
    TomlParser(TomlDocument& document, const ca::str::Utf8StringRef& input,
               const TomlParserOptions& options = TomlParserOptions());

    /// @brief 解析顶层 TOML 文档。
    /// @return 无错误返回 true；出错返回 false（首个错误可经 last_error() 取得）。
    bool run();

    /// @brief 首个解析错误。
    const ParseError& last_error() const noexcept;

private:
    // ---- 字节流游标 ----
    const u8* data_;
    usize byte_length_;
    usize pos_;
    SourceLocation loc_;
    bool failed_;
    ca::usize depth_;

    TomlDocument& document_;
    TomlParserOptions options_;
    ParseError error_;

    // 已显式定义过的 table header 完整路径（O(1) 精确重复检测）。
    // 路径编码为 segment1\x1Fsegment2\x1F...（\x1F 作分隔符，避免与 key 字符冲突）。
    std::unordered_set<std::string> defined_paths_;

    // 经 dotted-key（a.b.c = 1 的 a.b 前缀）创建的表路径：TOML 1.0 禁止用 [header]
    // 重开这类表（但允许作为中间路径下钻，如 [a.b.d]）。
    std::unordered_set<std::string> dotted_paths_;

    // inline table 字面量（x = {…}）所在路径：inline 表封闭不可变——禁止 [header]
    // 定向/下钻，也禁止 dotted-key 追加。
    std::unordered_set<std::string> inline_paths_;

    // 当前正在追加 key=value 的 table（标准表头或 root；inline/array of tables 由 caller 处理）。
    // 指向 document 内 TomlValue（Table），生命周期随 document。
    TomlValue* current_table_;

    // current_table_ 的全局路径（root 为空串），编码同 defined_paths_。
    // dotted-key 创建/下钻的表路径以此为前缀计算，保证与 header 路径同构可比。
    std::string current_table_path_;

    // ---- 基本字节操作 ----
    u8 peek() const noexcept;
    u8 peek_at(usize offset) const noexcept;
    void advance() noexcept;
    bool at_end() const noexcept;

    // ---- 空白 / 注释 / 行 ----
    void skip_inline_ws();         // 空格 + tab
    void skip_ws_comments_newlines();  // 任意空白/注释/换行
    bool skip_comment();           // # 到行尾
    bool skip_to_newline_or_end(); // 行尾空白 + 可选注释 + 换行；返回 false 表示行没结束

    // ---- 错误报告 ----
    void fail(SourceLocation loc, const char* message);

    // ---- 行级解析 ----
    void parse_document();
    bool parse_line();

    /// 解析 dotted key 序列（至少 1 段）。每段可能是 bare 或 quoted。
    /// out_segments 输出每个段的 Utf8StringRef（已 intern）。
    /// out_locs 输出每段起始位置（用于错误定位）。
    bool parse_key_path(std::vector<ca::str::Utf8StringRef>& out_segments,
                        std::vector<SourceLocation>& out_locs);

    /// 处理 [a.b.c] 或 [[a.b.c]] 行。已消费到 [ 或 [[。
    bool parse_table_header(bool array_of_tables);

    /// 处理 key = value 行。segments 是完整 dotted-key 路径。
    bool parse_key_value(const std::vector<ca::str::Utf8StringRef>& segments,
                         const std::vector<SourceLocation>& seg_locs);

    /// 标记完整路径（last_index 段）为已被 [header] 定义（segments 相对 root）。
    void mark_header_defined(const std::vector<ca::str::Utf8StringRef>& segments,
                            ca::usize last_index);

    /// 计算 segments[0..last_index] 相对 current_table_ 的全局路径（编码同 defined_paths_）。
    std::string global_path(const std::vector<ca::str::Utf8StringRef>& segments,
                            ca::usize last_index) const;

    // ---- 值解析 ----
    bool parse_value(TomlValue& out);
    bool parse_string_value(TomlValue& out);
    bool parse_basic_string(ca::str::Utf8StringBuilder& out);
    bool parse_literal_string(ca::str::Utf8StringBuilder& out);
    bool parse_multiline_basic(ca::str::Utf8StringBuilder& out);
    bool parse_multiline_literal(ca::str::Utf8StringBuilder& out);

    bool parse_number_or_datetime_value(TomlValue& out);
    bool parse_array(TomlValue& out);
    bool parse_inline_table(TomlValue& out);

    /// 是否在 offset 起位置精确匹配关键字（不消费）。
    bool match_keyword_at(usize offset, const char* kw) const;

    // ---- 字面量辅助 ----
    bool try_parse_datetime(const u8* begin, usize len, TomlDatetime& out, usize& consumed);
    bool parse_number_decimal(const u8* begin, usize len, TomlValue& out);
    bool parse_integer_radix(const u8* begin, usize len, int radix, TomlValue& out);
    bool parse_special_float(const u8* begin, usize len, TomlValue& out);

    // ---- 字符串解码辅助 ----
    static bool parse_hex4(const u8* p, u32& out);
    static bool parse_hex8(const u8* p, u32& out);
    /// 从 data[i] 开始解码一个 UTF-8 码点，成功返回 true 并推进 i。
    static bool decode_utf8(const u8* data, usize len, usize& i, u32& cp);
};

}  // namespace ca::toml
