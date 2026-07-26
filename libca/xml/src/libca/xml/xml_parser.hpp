#pragma once

/// @file xml_parser.hpp
/// @brief XML 递归下降解析器：XmlParser。把输入字节流构建为 XmlDocument。
/// @details XML 是定界符驱动的格式（`<...>`）。解析器单趟字节游标扫描：先处理 BOM /
///          XML 声明 / prolog（注释），再解析唯一根元素，最后处理 epilog（注释）。
///          所有字符串经 document.arena().intern(...) 入池。
/// @note 命名空间不特殊处理：`prefix:local` 整体作为名字。文本/属性值中的命名实体
///       （&lt; &gt; &amp; &apos; &quot;）与数字字符引用（&#DD; &#xHH;）解码；未知实体报错。
/// @warning DOCTYPE/DTD、非声明处理指令（PI）、多根元素、闭合标签不匹配、重复属性 → 报错。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "libca/xml/parse_error.hpp"
#include "libca/xml/source_location.hpp"
#include "libca/xml/xml_document.hpp"
#include "libca/xml/xml_node.hpp"

#include <vector>

namespace ca::xml {

/// @brief XML 解析选项。
struct XmlParserOptions {
    /// 最大元素嵌套深度，超出报错以防栈溢出。
    ca::usize max_depth = 1000;
    /// 丢弃元素之间的纯空白文本节点（默认开）。关掉可得逐字节保真的 DOM。
    /// @note 含非空白字符的文本节点永远完整保留（混合内容不受影响）。
    bool trim_whitespace = true;
};

/// @brief XML 递归下降解析器，把输入构建为 XmlDocument。
class XmlParser {
public:
    /// @brief 构造解析器。document 持有 arena + root；解析期间所有字符串入 arena。
    ///        输入视图须在使用期内有效（零拷贝）。
    XmlParser(XmlDocument& document, const ca::str::Utf8StringRef& input,
              const XmlParserOptions& options = XmlParserOptions());

    /// @brief 解析顶层 XML 文档。
    /// @return 无错误返回 true；出错返回 false（首个错误可经 last_error() 取得）。
    bool run();

    /// @brief 首个解析错误。
    const ParseError& last_error() const noexcept;

private:
    // ---- 字节流游标 ----
    const u8* data_;
    ca::usize byte_length_;
    ca::usize pos_;
    SourceLocation loc_;
    bool failed_;
    ca::usize depth_;

    XmlDocument& document_;
    XmlParserOptions options_;
    ParseError error_;

    // ---- 基本字节操作 ----
    u8 peek() const noexcept;
    u8 peek_at(ca::usize offset) const noexcept;
    void advance() noexcept;
    bool at_end() const noexcept;
    // 从当前位置起是否精确匹配字面量（不消费）。
    bool starts_with(const char* literal) const noexcept;

    // ---- 空白 / 错误 ----
    bool skip_ws();  // 跳过 XML 空白（空格/\t/\r/\n），返回是否跳过了至少一个
    void fail(SourceLocation loc, const char* message);
    void fail_str(SourceLocation loc, const std::string& message);

    // ---- 顶层结构 ----
    void skip_bom();
    bool parse_declaration();  // 可选 <?xml ... ?>
    // 解析 prolog/epilog 的杂项（空白 + 注释）到 out；遇元素起始或 EOF 停。
    // DOCTYPE/PI 报错。
    bool parse_misc(std::vector<XmlNode>& out);
    bool parse_element(XmlNode& out);

    // ---- 元素内部 ----
    bool parse_name(ca::str::Utf8StringRef& out);
    bool parse_attributes(XmlNode& element);
    bool parse_attr_value(ca::str::Utf8StringRef& out);
    bool parse_content(XmlNode& element);  // 解析子节点直到 </
    bool parse_end_tag(const ca::str::Utf8StringRef& expected_name);

    // ---- 叶子节点 ----
    bool parse_comment(ca::str::Utf8StringRef& out);
    bool parse_cdata(ca::str::Utf8StringRef& out);
    // 解析一个引用 &...; 并把解码结果追加到 builder。
    bool parse_reference(ca::str::Utf8StringBuilder& out);

    // ---- 字符判定 / 编码 ----
    static bool is_ws(u8 c) noexcept;
    static bool is_name_start(u8 c) noexcept;
    static bool is_name_char(u8 c) noexcept;
    // 把码点 UTF-8 编码追加到 builder；非法码点返回 false。
    static bool encode_utf8(u32 cp, ca::str::Utf8StringBuilder& out);
};

}  // namespace ca::xml
