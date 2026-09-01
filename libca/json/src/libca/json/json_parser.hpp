#pragma once

/// @file json_parser.hpp
/// @brief JSON 递归下降解析器：JsonParser。驱动 JsonHandler 发出 SAX 事件。
/// @details 词法与语法分析合一（JSON 词法简单，单字节即可定 token）。解析过程对输入做单趟扫描，
///          按需回调 handler。首个错误经 handler.on_error 报告后立即停止。
/// @note 字符串事件（on_string / on_object_key）的 Utf8StringRef 指向构造时传入的 arena
///       内副本——所有解码后的字符串经 `arena.intern(...)` 入池。SAX 用户因此不持有 owning
///       字符串；自定义 handler 拿到的 ref 生命周期绑定到 arena（由 JsonReader 路径下的
///       JsonDocument 持有，或纯 SAX 用户自管 arena）。

#include "libca/core/datatype.hpp"

#include "libca/json/json_handler.hpp"
#include "libca/json/parse_error.hpp"
#include "libca/json/source_location.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace ca::json {

/// @brief JsonParser 宽松选项（默认全部 false，即严格 RFC 8259）。
struct JsonParserOptions
{
    /// 允许尾随逗号：`[1,2,]` / `{"a":1,}`。
    bool allow_trailing_comma = false;
    /// 允许 `//` 和 `/* */` 注释（非标准扩展）。
    bool allow_comments = false;
    /// 最大嵌套深度（数组/对象），超出报错以防栈溢出。
    ca::usize max_depth = 1000;
};

/// @brief 递归下降 JSON 解析器，把输入驱动为 JsonHandler 事件。
class JsonParser
{
public:
    /// @brief 构造解析器。输入视图与 arena 须在使用期内有效。
    /// @details 解析出的字符串经 `arena.intern(...)` 入池，事件回调里的 Utf8StringRef
    ///          指向 arena 内副本（与输入视图生命周期解耦）。
    JsonParser(ca::str::Utf8StringRef input, JsonHandler& handler, ca::str::Utf8StringArena& arena,
               const JsonParserOptions& options = JsonParserOptions());

    /// @brief 解析顶层 JSON 值。
    /// @return 无错误返回 true；出错返回 false（首个错误经 handler.on_error 已报告，
    ///         并可经 last_error() 取得）。
    bool parse();

    /// @brief 当前扫描位置。
    SourceLocation location() const noexcept;

    /// @brief 首个解析错误（仅 parse() 返回 false 时有效）。
    const ParseError& last_error() const noexcept;

private:
    // ---- 字节流游标 ----
    const u8*      data_;
    usize          byte_length_;
    usize          pos_;
    SourceLocation loc_;
    bool           failed_;
    ca::usize      depth_;

    JsonHandler&              handler_;
    ca::str::Utf8StringArena& arena_;
    JsonParserOptions         options_;
    ParseError                error_;   // 首个错误

    // ---- 基本字节操作 ----
    u8   peek() const noexcept;        // 当前字节，EOF 返回 0
    u8   peek_next() const noexcept;   // 下一个字节，EOF 返回 0
    void advance() noexcept;           // 前进一个字节，同步更新 loc_
    bool at_end() const noexcept;

    // ---- 空白 / 注释 ----
    void skip_ws_and_comments();

    // ---- 错误报告 ----
    void fail(SourceLocation loc, const char* message);

    // ---- 递归下降 ----
    void parse_value();
    void parse_array();
    void parse_object();
    void parse_string_value();
    void parse_number();
    void parse_literal(const char* text, ca::usize len, bool value, const char* kind);
    // 解析字符串字面量并 intern 到 arena；返回是否成功（失败已 fail）。
    bool parse_string_into(ca::str::Utf8StringRef& out);
};

}   // namespace ca::json
