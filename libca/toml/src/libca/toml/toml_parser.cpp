#include "libca/toml/toml_parser.hpp"

#include "libca/str/utf8_util.hpp"

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace ca::toml {

// 单元分隔符（\x1F）：用作 defined_headers_ 路径拼接的分隔，用户极少在 key 中主动写入。
static const char kPathSep = '\x1F';

// ============================================================================
// 构造与游标
// ============================================================================

TomlParser::TomlParser(TomlDocument& document, const ca::str::Utf8StringRef& input,
                       const TomlParserOptions& options)
    : data_(input.data())
    , byte_length_(input.byte_length())
    , pos_(0)
    , loc_()
    , failed_(false)
    , depth_(0)
    , document_(document)
    , options_(options)
    , error_()
    , current_table_(&document_.root())
{
    if (data_ == nullptr) {
        data_        = reinterpret_cast<const u8*>("");
        byte_length_ = 0;
    }
    if (document_.root().type() != TomlType::Table) {
        document_.root() = TomlValue::make_table();
    }
}

const ParseError& TomlParser::last_error() const noexcept
{
    return error_;
}

u8 TomlParser::peek() const noexcept
{
    return pos_ < byte_length_ ? data_[pos_] : 0;
}

u8 TomlParser::peek_at(usize offset) const noexcept
{
    return pos_ + offset < byte_length_ ? data_[pos_ + offset] : 0;
}

bool TomlParser::at_end() const noexcept
{
    return pos_ >= byte_length_;
}

void TomlParser::advance() noexcept
{
    if (pos_ >= byte_length_)
        return;
    const u8 c = data_[pos_];
    ++pos_;
    ++loc_.offset;
    if (c == '\n') {
        ++loc_.line;
        loc_.column = 1;
    }
    else {
        ++loc_.column;
    }
}

void TomlParser::fail(SourceLocation loc, const char* message)
{
    if (failed_)
        return;
    failed_         = true;
    error_.location = loc;
    error_.message  = ca::str::Utf8String::from_cstr(message);
}

// ============================================================================
// 空白 / 注释 / 行
// ============================================================================

void TomlParser::skip_inline_ws()
{
    while (!failed_ && pos_ < byte_length_) {
        const u8 c = data_[pos_];
        if (c == ' ' || c == '\t') {
            advance();
        }
        else {
            return;
        }
    }
}

bool TomlParser::skip_comment()
{
    if (peek() != '#')
        return false;
    while (pos_ < byte_length_ && data_[pos_] != '\n' && data_[pos_] != '\r')
        advance();
    return true;
}

bool TomlParser::skip_to_newline_or_end()
{
    skip_inline_ws();
    if (failed_)
        return false;
    if (peek() == '#')
        skip_comment();
    if (failed_)
        return false;
    if (at_end())
        return true;
    const u8 c = peek();
    if (c == '\n') {
        advance();
        return true;
    }
    if (c == '\r') {
        advance();
        if (peek() == '\n')
            advance();
        return true;
    }
    fail(loc_, "expected newline or end of line");
    return false;
}

void TomlParser::skip_ws_comments_newlines()
{
    while (!failed_ && pos_ < byte_length_) {
        const u8 c = data_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        }
        else if (c == '#') {
            skip_comment();
        }
        else {
            return;
        }
    }
}

// ============================================================================
// 入口
// ============================================================================

bool TomlParser::run()
{
    // TOML 文件必须是合法 UTF-8。入口整体校验一次——字符串值经 arena intern 时
    // 非法序列会静默变空串、解析"成功"，数据无声丢失（json/ini 已修过同类问题）。
    if (!ca::str::utf8_is_valid(data_, byte_length_)) {
        fail(SourceLocation{0, 1, 1}, "invalid UTF-8 in TOML text");
        return false;
    }
    // TOML 1.0 允许 UTF-8 BOM。
    if (byte_length_ >= 3 && data_[0] == 0xEF && data_[1] == 0xBB && data_[2] == 0xBF) {
        advance();
        advance();
        advance();
    }
    parse_document();
    return !failed_;
}

void TomlParser::parse_document()
{
    while (!failed_ && !at_end()) {
        skip_ws_comments_newlines();
        if (failed_ || at_end())
            break;
        parse_line();
    }
}

bool TomlParser::parse_line()
{
    if (failed_)
        return false;
    const u8 c = peek();
    if (c == '[') {
        if (peek_at(1) == '[') {
            advance();
            advance();   // [[
            return parse_table_header(true);
        }
        advance();   // [
        return parse_table_header(false);
    }
    // key-value 行：先解析完整 dotted key 路径。
    std::vector<ca::str::Utf8StringRef> segments;
    std::vector<SourceLocation>         seg_locs;
    if (!parse_key_path(segments, seg_locs))
        return false;
    return parse_key_value(segments, seg_locs);
}

static bool is_bare_key_char(u8 c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '-';
}

bool TomlParser::parse_key_path(std::vector<ca::str::Utf8StringRef>& out_segments,
                                std::vector<SourceLocation>&         out_locs)
{
    while (!failed_) {
        skip_inline_ws();
        if (failed_)
            return false;
        SourceLocation             seg_loc = loc_;
        ca::str::Utf8StringBuilder sb;
        const u8                   c = peek();
        if (c == '"') {
            if (!parse_basic_string(sb))
                return false;
        }
        else if (c == '\'') {
            if (!parse_literal_string(sb))
                return false;
        }
        else if (is_bare_key_char(c)) {
            while (is_bare_key_char(peek())) {
                sb.append(&data_[pos_], 1);
                advance();
            }
            if (sb.byte_length() == 0) {
                fail(loc_, "empty key segment");
                return false;
            }
        }
        else {
            fail(loc_, "expected key");
            return false;
        }
        ca::str::Utf8String    built = sb.build_or_empty();
        ca::str::Utf8StringRef ref   = document_.arena().intern(built);
        out_segments.push_back(ref);
        out_locs.push_back(seg_loc);

        skip_inline_ws();
        if (failed_)
            return false;
        if (peek() == '.') {
            advance();
            continue;
        }
        return true;
    }
    return false;
}

// ============================================================================
// Table header 处理
// ============================================================================

bool TomlParser::parse_table_header(bool array_of_tables)
{
    SourceLocation header_loc = loc_;
    // 已经消费了 [ 或 [[。
    std::vector<ca::str::Utf8StringRef> segments;
    std::vector<SourceLocation>         seg_locs;
    if (!parse_key_path(segments, seg_locs))
        return false;
    skip_inline_ws();
    if (failed_)
        return false;
    // 期望闭合括号
    if (array_of_tables) {
        if (peek() != ']' || peek_at(1) != ']') {
            fail(loc_, "expected ']]' to close array-of-tables header");
            return false;
        }
        advance();
        advance();
    }
    else {
        if (peek() != ']') {
            fail(loc_, "expected ']' to close table header");
            return false;
        }
        advance();
    }
    if (!skip_to_newline_or_end())
        return false;

    // 构造本次 header 的完整路径串（用于重复检测）。
    std::string            this_path;
    std::vector<ca::usize> prefix_len(segments.size());
    for (ca::usize i = 0; i < segments.size(); ++i) {
        if (i > 0)
            this_path.push_back(kPathSep);
        this_path.append(reinterpret_cast<const char*>(segments[i].data()),
                         segments[i].byte_length());
        prefix_len[i] = this_path.size();
    }
    // 重复检测（O(1)）：
    //   - 普通 [a.b]：若 a.b 已被 header 显式定义过（defined_paths_）报错。
    //     注意「已被子表头隐式创建」（如 [a.b] 之后再写 [a]）是合法的：TOML 1.0
    //     明确允许后置定义超表（spec：defining a super-table afterward is ok）。
    //   - 数组表 [[a.b]]：每个 [[a.b]] 都追加一个新元素，不视为重复；
    //     但若 a.b 已被普通 [header] 定义过，或已存在为非 array，报错。
    if (!array_of_tables) {
        if (defined_paths_.count(this_path) != 0) {
            fail(header_loc, "table redefined");
            return false;
        }
    }
    else {
        // [[a.b]]：若 a.b 已被普通 header 定义，报错；若 a.b 已是 array 则正常追加。
        // 这里只检测"是 array or 不存在"——若存在但非 array，会在下面遍历时报错。
    }

    // 组织：从 root 开始走路径，按 array_of_tables 决定末端行为。
    TomlValue* cur = &document_.root();
    for (ca::usize i = 0; i < segments.size() && !failed_; ++i) {
        const bool                    is_last = (i + 1 == segments.size());
        const ca::str::Utf8StringRef& key     = segments[i];
        if (cur->type() != TomlType::Table) {
            fail(seg_locs[i], "expected table when traversing header path");
            return false;
        }
        TomlValue* existing = cur->find(key);
        if (is_last && array_of_tables) {
            // 末端 [[x]]：必须是数组（首次则创建），追加一个新 Table 元素，current 指向它。
            if (existing == nullptr) {
                TomlValue arr = TomlValue::make_array();
                arr.append(TomlValue::make_table());
                cur->set(key, std::move(arr));
                existing = cur->find(key);
            }
            else {
                if (!existing->is_array()) {
                    fail(header_loc, "key already defined as non-array, cannot use [[ ]]");
                    return false;
                }
                // 数组表：检查是否已被普通 header 定义为同路径
                // （上面检测已处理 this_path 与 defined_headers 完全相同的情况）
                existing->append(TomlValue::make_table());
            }
            current_table_ = &existing->as_array().back();
            cur            = current_table_;
        }
        else if (existing == nullptr) {
            // 中间段或末端普通表：自动创建 Table。
            cur->set(key, TomlValue::make_table());
            cur = cur->find(key);
            if (is_last)
                current_table_ = cur;
        }
        else {
            // 已存在节点。
            if (is_last) {
                // 普通表头 [x]：上面重复检测已确认不是 header 重复。
                if (existing->is_table()) {
                    // TOML 1.0：dotted-key 创建的表与 inline table 封闭不可变，
                    // 不能被 [header] 定向重开；仅 header 遍历隐式创建的超表可后置定义。
                    if (dotted_paths_.count(this_path) != 0) {
                        fail(header_loc, "cannot define [table] for table created via dotted keys");
                        return false;
                    }
                    if (inline_paths_.count(this_path) != 0) {
                        fail(header_loc, "cannot define [table] for inline table");
                        return false;
                    }
                    current_table_ = existing;
                }
                else if (existing->is_array()) {
                    // 是 [[x]] 创建的数组：[x] header 不能再定义同名普通表。
                    fail(header_loc, "cannot define table with same name as array of tables");
                    return false;
                }
                else {
                    fail(header_loc, "key already defined as scalar value");
                    return false;
                }
                cur = current_table_;
            }
            else {
                // 中间段已存在：继续下钻。
                if (existing->is_array()) {
                    // 数组表的中间路径：归属最近一个元素。
                    cur = &existing->as_array().back();
                }
                else if (existing->is_table()) {
                    // inline table 内部禁止用 [header] 定义子表（inline 封闭不可变）；
                    // dotted-key 创建的表允许作为中间路径（如 [a.b] 创建后 [a.b.c.d]）。
                    const std::string prefix = this_path.substr(0, prefix_len[i]);
                    if (inline_paths_.count(prefix) != 0) {
                        fail(seg_locs[i], "cannot define sub-tables of an inline table");
                        return false;
                    }
                    cur = existing;
                }
                else {
                    fail(seg_locs[i], "expected table along header path, found scalar");
                    return false;
                }
            }
        }
    }

    // 标记完整路径已被 [header] 定义（用于后续重复检测），并更新当前表上下文路径。
    mark_header_defined(segments, segments.size() - 1);
    current_table_path_ = this_path;
    return !failed_;
}

bool TomlParser::parse_key_value(const std::vector<ca::str::Utf8StringRef>& segments,
                                 const std::vector<SourceLocation>&         seg_locs)
{
    if (failed_)
        return false;
    // 当前位置应在 '=' 前。
    skip_inline_ws();
    if (failed_)
        return false;
    if (peek() != '=') {
        fail(loc_, "expected '=' after key");
        return false;
    }
    advance();   // '='
    skip_inline_ws();
    if (failed_)
        return false;

    // 解析 value。
    TomlValue value;
    if (!parse_value(value))
        return false;

    // 在 current_table_ 中按 dotted key 路径插入。
    // 前 N-1 段：创建/下钻 table；末段：在父表里 set 叶子 key。
    TomlValue* parent = current_table_;
    for (ca::usize i = 0; i + 1 < segments.size() && !failed_; ++i) {
        const ca::str::Utf8StringRef& key = segments[i];
        if (parent->type() != TomlType::Table) {
            fail(seg_locs[i], "dotted key descends into non-table");
            return false;
        }
        TomlValue* existing = parent->find(key);
        if (existing == nullptr) {
            parent->set(key, TomlValue::make_table());
            existing = parent->find(key);
            // 记录 dotted-key 创建的表：TOML 1.0 禁止后续 [header] 定向重开
            // （但允许作为 header 中间路径下钻，也允许同表内继续 dotted-key 追加）。
            dotted_paths_.insert(global_path(segments, i));
        }
        else if (!existing->is_table()) {
            fail(seg_locs[i], "dotted key segment conflicts with non-table value");
            return false;
        }
        else if (defined_paths_.count(global_path(segments, i)) != 0) {
            // TOML 1.0：不能用 dotted-key 重开 [header] 已定义的表。
            fail(seg_locs[i], "cannot extend header-defined table via dotted keys");
            return false;
        }
        else if (inline_paths_.count(global_path(segments, i)) != 0) {
            // inline table 封闭不可变：禁止 dotted-key 追加。
            fail(seg_locs[i], "cannot extend inline table via dotted keys");
            return false;
        }
        parent = existing;
    }
    if (failed_)
        return false;

    const ca::str::Utf8StringRef& leaf = segments.back();
    if (parent->type() != TomlType::Table) {
        fail(seg_locs.back(), "cannot insert key into non-table");
        return false;
    }
    if (parent->find(leaf) != nullptr) {
        fail(seg_locs.back(), "duplicate key");
        return false;
    }
    // key-value 位置的 table 值只能是 inline 字面量：记录路径，
    // 禁止后续 [header] 定向/下钻与 dotted-key 追加（inline 封闭不可变）。
    if (value.is_table()) {
        inline_paths_.insert(global_path(segments, segments.size() - 1));
    }
    parent->set(leaf, std::move(value));
    if (!skip_to_newline_or_end())
        return false;
    return !failed_;
}

// 记录 header 完整路径串（kPathSep 分隔），供后续同路径 header 报重复。
// 隐式父表不记录：后置定义超表合法（[a.b] 之后可再写 [a]）。
// header 的 segments 相对 root，不带 current_table_ 前缀。
void TomlParser::mark_header_defined(const std::vector<ca::str::Utf8StringRef>& segments,
                                     ca::usize                                  last_index)
{
    std::string path;
    for (ca::usize i = 0; i <= last_index; ++i) {
        if (i > 0)
            path.push_back(kPathSep);
        path.append(reinterpret_cast<const char*>(segments[i].data()), segments[i].byte_length());
    }
    defined_paths_.insert(std::move(path));
}

// dotted-key 的 segments 相对 current_table_，须补前缀才能与 header 路径同构比较：
// [a] 下写 b.c = 1 与 header [a.b] 对应同一路径 a\x1Fb。
std::string TomlParser::global_path(const std::vector<ca::str::Utf8StringRef>& segments,
                                    ca::usize                                  last_index) const
{
    std::string path = current_table_path_;
    for (ca::usize i = 0; i <= last_index; ++i) {
        if (!path.empty())
            path.push_back(kPathSep);
        path.append(reinterpret_cast<const char*>(segments[i].data()), segments[i].byte_length());
    }
    return path;
}

// ============================================================================
// Value 解析
// ============================================================================

bool TomlParser::parse_value(TomlValue& out)
{
    if (failed_)
        return false;
    const u8 c = peek();
    if (c == 0) {
        fail(loc_, "unexpected end of input");
        return false;
    }

    // 字符串字面量（含多行）
    if (c == '"') {
        if (peek_at(1) == '"' && peek_at(2) == '"') {
            advance();
            advance();
            advance();   // """
            ca::str::Utf8StringBuilder sb;
            if (!parse_multiline_basic(sb))
                return false;
            out = TomlValue::make_string(document_.arena().intern(sb.build_or_empty()));
            return !failed_;
        }
        return parse_string_value(out);
    }
    if (c == '\'') {
        if (peek_at(1) == '\'' && peek_at(2) == '\'') {
            advance();
            advance();
            advance();   // '''
            ca::str::Utf8StringBuilder sb;
            if (!parse_multiline_literal(sb))
                return false;
            out = TomlValue::make_string(document_.arena().intern(sb.build_or_empty()));
            return !failed_;
        }
        return parse_string_value(out);
    }

    // Array
    if (c == '[') {
        return parse_array(out);
    }
    // Inline table
    if (c == '{') {
        return parse_inline_table(out);
    }

    // bool
    if (c == 't' || c == 'f') {
        // 尝试 true/false
        if (peek_at(0) == 't' && peek_at(1) == 'r' && peek_at(2) == 'u' && peek_at(3) == 'e') {
            advance();
            advance();
            advance();
            advance();
            out = TomlValue::make_boolean(true);
            return true;
        }
        if (peek_at(0) == 'f' && peek_at(1) == 'a' && peek_at(2) == 'l' && peek_at(3) == 's' &&
            peek_at(4) == 'e') {
            advance();
            advance();
            advance();
            advance();
            advance();
            out = TomlValue::make_boolean(false);
            return true;
        }
        fail(loc_, "invalid literal");
        return false;
    }

    // inf / nan（可能带符号）—— 先看符号再决定。
    // 数值或 datetime：以 -/+/数字开头。
    if (c == '-' || c == '+') {
        // 可能是 +/-inf / +/-nan，也可能是带符号数值或 datetime。
        const u8 n1 = peek_at(1);
        if (n1 == 'i' && match_keyword_at(1, "inf")) {
            return parse_special_float(&data_[pos_], 4, out);
        }
        if (n1 == 'n' && match_keyword_at(1, "nan")) {
            return parse_special_float(&data_[pos_], 4, out);
        }
    }
    else {
        // 无符号 inf/nan
        if (c == 'i' && match_keyword_at(0, "inf")) {
            return parse_special_float(&data_[pos_], 3, out);
        }
        if (c == 'n' && match_keyword_at(0, "nan")) {
            return parse_special_float(&data_[pos_], 3, out);
        }
    }
    return parse_number_or_datetime_value(out);
}

bool TomlParser::match_keyword_at(usize offset, const char* kw) const
{
    const usize kwlen = std::strlen(kw);
    if (pos_ + offset + kwlen > byte_length_)
        return false;
    for (usize i = 0; i < kwlen; ++i) {
        if (data_[pos_ + offset + i] != static_cast<u8>(kw[i]))
            return false;
    }
    return true;
}

bool TomlParser::parse_string_value(TomlValue& out)
{
    SourceLocation             start = loc_;
    ca::str::Utf8StringBuilder sb;
    const u8                   c  = peek();
    bool                       ok = false;
    if (c == '"')
        ok = parse_basic_string(sb);
    else if (c == '\'')
        ok = parse_literal_string(sb);
    else {
        fail(start, "expected string");
        return false;
    }
    if (!ok)
        return false;
    out = TomlValue::make_string(document_.arena().intern(sb.build_or_empty()));
    return !failed_;
}

// 解析 basic string（消费开/闭引号）。
bool TomlParser::parse_basic_string(ca::str::Utf8StringBuilder& out)
{
    SourceLocation start = loc_;
    assert(peek() == '"');
    advance();   // 开引号
    while (!failed_) {
        if (at_end()) {
            fail(start, "unterminated string");
            return false;
        }
        const u8 c = peek();
        if (c == '"') {
            advance();
            break;
        }
        if (c == '\n' || c == '\r') {
            fail(loc_, "unterminated string: newline in basic string");
            return false;
        }
        if (c == '\\') {
            advance();
            if (at_end()) {
                fail(start, "unterminated escape");
                return false;
            }
            const u8 esc = peek();
            advance();
            switch (esc) {
            case '"': out.append("\""); break;
            case '\\': out.append("\\"); break;
            case 'b': out.append("\b"); break;
            case 't': out.append("\t"); break;
            case 'n': out.append("\n"); break;
            case 'f': out.append("\f"); break;
            case 'r': out.append("\r"); break;
            case 'u':
            {
                if (pos_ + 4 > byte_length_) {
                    fail(start, "incomplete \\u escape");
                    return false;
                }
                u32 cp = 0;
                if (!parse_hex4(data_ + pos_, cp)) {
                    fail(start, "invalid \\u escape");
                    return false;
                }
                for (int i = 0; i < 4; ++i)
                    advance();
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    // 需要紧跟 \uXXXX 低代理
                    if (peek() == '\\' && peek_at(1) == 'u') {
                        advance();
                        advance();
                        if (pos_ + 4 > byte_length_) {
                            fail(start, "incomplete low surrogate");
                            return false;
                        }
                        u32 low = 0;
                        if (!parse_hex4(data_ + pos_, low)) {
                            fail(start, "invalid low surrogate");
                            return false;
                        }
                        for (int i = 0; i < 4; ++i)
                            advance();
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail(start, "expected low surrogate");
                            return false;
                        }
                        u32 full = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        if (!out.append_code_point(full)) {
                            fail(start, "invalid code point");
                            return false;
                        }
                    }
                    else {
                        fail(start, "dangling high surrogate");
                        return false;
                    }
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail(start, "unexpected low surrogate");
                    return false;
                }
                else {
                    if (!out.append_code_point(cp)) {
                        fail(start, "invalid code point");
                        return false;
                    }
                }
                break;
            }
            case 'U':
            {
                if (pos_ + 8 > byte_length_) {
                    fail(start, "incomplete \\U escape");
                    return false;
                }
                u32 cp = 0;
                if (!parse_hex8(data_ + pos_, cp)) {
                    fail(start, "invalid \\U escape");
                    return false;
                }
                for (int i = 0; i < 8; ++i)
                    advance();
                if (cp >= 0xD800 && cp <= 0xDFFF) {
                    fail(start, "surrogate in \\U escape");
                    return false;
                }
                if (!out.append_code_point(cp)) {
                    fail(start, "invalid code point");
                    return false;
                }
                break;
            }
            default: fail(start, "invalid escape character"); return false;
            }
        }
        else if (c < 0x20 && c != '\t') {
            fail(loc_, "unescaped control character in string");
            return false;
        }
        else {
            out.append(&c, 1);
            advance();
        }
    }
    return !failed_;
}

bool TomlParser::parse_literal_string(ca::str::Utf8StringBuilder& out)
{
    SourceLocation start = loc_;
    assert(peek() == '\'');
    advance();
    while (!failed_) {
        if (at_end()) {
            fail(start, "unterminated string");
            return false;
        }
        const u8 c = peek();
        if (c == '\'') {
            advance();
            break;
        }
        if (c == '\n' || c == '\r') {
            fail(loc_, "unterminated string: newline in literal string");
            return false;
        }
        if (c < 0x20 && c != '\t') {
            fail(loc_, "unescaped control character in string");
            return false;
        }
        out.append(&c, 1);
        advance();
    }
    return !failed_;
}

bool TomlParser::parse_multiline_basic(ca::str::Utf8StringBuilder& out)
{
    // 调用方已消费 """。
    SourceLocation start = loc_;
    // 开头若紧跟换行，该换行被忽略。
    if (peek() == '\r') {
        advance();
        if (peek() == '\n')
            advance();
    }
    else if (peek() == '\n') {
        advance();
    }
    while (!failed_) {
        if (at_end()) {
            fail(start, "unterminated multiline string");
            return false;
        }
        const u8 c = peek();
        // 闭合 """
        if (c == '"' && peek_at(1) == '"' && peek_at(2) == '"') {
            // 允许闭合前出现 1~2 个额外的 "（属于字符串内容）。
            // TOML："""str""" 允许最多两个相邻引号在闭合前。
            // 处理：若 4-5 个引号，则把多余 1-2 个视为内容。
            usize extra_quotes = 0;
            if (peek_at(3) == '"')
                extra_quotes = 1;
            if (peek_at(3) == '"' && peek_at(4) == '"')
                extra_quotes = 2;
            for (usize k = 0; k < extra_quotes; ++k) {
                out.append("\"");
            }
            for (usize k = 0; k < 3 + extra_quotes; ++k)
                advance();
            break;
        }
        if (c == '\\') {
            // 行尾反斜杠续行：当 \ 后面到行尾只有空白时，吃掉换行和下一行前导空白。
            // 也可能是普通转义。判断：\ 后紧跟换行或空白直到换行 → 续行。
            usize lookahead      = pos_ + 1;
            bool  only_ws_to_eol = false;
            while (lookahead < byte_length_) {
                const u8 lc = data_[lookahead];
                if (lc == ' ' || lc == '\t' || lc == '\r') {
                    ++lookahead;
                    continue;
                }
                if (lc == '\n') {
                    only_ws_to_eol = true;
                }
                break;
            }
            if (only_ws_to_eol) {
                advance();   // 消费 \
                // 跳过空白直到换行（含换行）和下一行前导空白
                while (!at_end() &&
                       (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n'))
                    advance();
                continue;
            }
            // 普通转义：复用单行逻辑
            advance();   // backslash
            if (at_end()) {
                fail(start, "unterminated escape");
                return false;
            }
            const u8 esc = peek();
            advance();
            switch (esc) {
            case '"': out.append("\""); break;
            case '\\': out.append("\\"); break;
            case 'b': out.append("\b"); break;
            case 't': out.append("\t"); break;
            case 'n': out.append("\n"); break;
            case 'f': out.append("\f"); break;
            case 'r': out.append("\r"); break;
            case 'u':
            {
                if (pos_ + 4 > byte_length_) {
                    fail(start, "incomplete \\u escape");
                    return false;
                }
                u32 cp = 0;
                if (!parse_hex4(data_ + pos_, cp)) {
                    fail(start, "invalid \\u escape");
                    return false;
                }
                for (int i = 0; i < 4; ++i)
                    advance();
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (peek() == '\\' && peek_at(1) == 'u') {
                        advance();
                        advance();
                        if (pos_ + 4 > byte_length_) {
                            fail(start, "incomplete low surrogate");
                            return false;
                        }
                        u32 low = 0;
                        if (!parse_hex4(data_ + pos_, low)) {
                            fail(start, "invalid low surrogate");
                            return false;
                        }
                        for (int i = 0; i < 4; ++i)
                            advance();
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail(start, "expected low surrogate");
                            return false;
                        }
                        u32 full = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        if (!out.append_code_point(full)) {
                            fail(start, "invalid code point");
                            return false;
                        }
                    }
                    else {
                        fail(start, "dangling high surrogate");
                        return false;
                    }
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail(start, "unexpected low surrogate");
                    return false;
                }
                else {
                    if (!out.append_code_point(cp)) {
                        fail(start, "invalid code point");
                        return false;
                    }
                }
                break;
            }
            case 'U':
            {
                if (pos_ + 8 > byte_length_) {
                    fail(start, "incomplete \\U escape");
                    return false;
                }
                u32 cp = 0;
                if (!parse_hex8(data_ + pos_, cp)) {
                    fail(start, "invalid \\U escape");
                    return false;
                }
                for (int i = 0; i < 8; ++i)
                    advance();
                if (cp >= 0xD800 && cp <= 0xDFFF) {
                    fail(start, "surrogate in \\U escape");
                    return false;
                }
                if (!out.append_code_point(cp)) {
                    fail(start, "invalid code point");
                    return false;
                }
                break;
            }
            default: fail(start, "invalid escape character"); return false;
            }
        }
        else if (c == '\r') {
            // 规范化 CRLF/CR 为 \n
            advance();
            if (peek() == '\n')
                advance();
            out.append("\n");
        }
        else if (c < 0x20 && c != '\t' && c != '\n') {
            fail(loc_, "unescaped control character in string");
            return false;
        }
        else {
            out.append(&c, 1);
            advance();
        }
    }
    return !failed_;
}

bool TomlParser::parse_multiline_literal(ca::str::Utf8StringBuilder& out)
{
    // 调用方已消费 '''。
    SourceLocation start = loc_;
    if (peek() == '\r') {
        advance();
        if (peek() == '\n')
            advance();
    }
    else if (peek() == '\n') {
        advance();
    }
    while (!failed_) {
        if (at_end()) {
            fail(start, "unterminated multiline literal string");
            return false;
        }
        const u8 c = peek();
        if (c == '\'' && peek_at(1) == '\'' && peek_at(2) == '\'') {
            usize extra_quotes = 0;
            if (peek_at(3) == '\'')
                extra_quotes = 1;
            if (peek_at(3) == '\'' && peek_at(4) == '\'')
                extra_quotes = 2;
            for (usize k = 0; k < extra_quotes; ++k)
                out.append("'");
            for (usize k = 0; k < 3 + extra_quotes; ++k)
                advance();
            break;
        }
        if (c == '\r') {
            advance();
            if (peek() == '\n')
                advance();
            out.append("\n");
        }
        else if (c < 0x20 && c != '\t' && c != '\n') {
            fail(loc_, "unescaped control character in string");
            return false;
        }
        else {
            out.append(&c, 1);
            advance();
        }
    }
    return !failed_;
}

// ============================================================================
// Array
// ============================================================================

bool TomlParser::parse_array(TomlValue& out)
{
    SourceLocation start = loc_;
    assert(peek() == '[');
    advance();
    ++depth_;
    if (depth_ > options_.max_depth) {
        fail(start, "nesting too deep");
        --depth_;
        return false;
    }
    out = TomlValue::make_array();
    while (!failed_) {
        skip_ws_comments_newlines();
        if (failed_)
            break;
        if (peek() == ']') {
            advance();
            break;
        }
        if (at_end()) {
            fail(start, "unterminated array");
            break;
        }
        TomlValue elem;
        if (!parse_value(elem))
            break;
        out.append(std::move(elem));
        skip_ws_comments_newlines();
        if (failed_)
            break;
        const u8 c = peek();
        if (c == ',') {
            advance();
            continue;
        }
        if (c == ']') {
            advance();
            break;
        }
        fail(loc_, "expected ',' or ']' in array");
        break;
    }
    --depth_;
    return !failed_;
}

// ============================================================================
// Inline Table
// ============================================================================

bool TomlParser::parse_inline_table(TomlValue& out)
{
    SourceLocation start = loc_;
    assert(peek() == '{');
    advance();
    ++depth_;
    if (depth_ > options_.max_depth) {
        fail(start, "nesting too deep");
        --depth_;
        return false;
    }
    out = TomlValue::make_table();
    skip_inline_ws();
    if (failed_) {
        --depth_;
        return false;
    }
    if (peek() == '}') {
        advance();
        --depth_;
        return true;
    }

    while (!failed_) {
        // 解析 dotted key
        std::vector<ca::str::Utf8StringRef> segments;
        std::vector<SourceLocation>         seg_locs;
        if (!parse_key_path(segments, seg_locs)) {
            break;
        }
        skip_inline_ws();
        if (failed_)
            break;
        if (peek() != '=') {
            fail(loc_, "expected '=' in inline table");
            break;
        }
        advance();
        skip_inline_ws();
        if (failed_)
            break;
        TomlValue val;
        if (!parse_value(val))
            break;
        // 在 inline table 里按 dotted key 插入
        TomlValue* parent  = &out;
        bool       path_ok = true;
        for (ca::usize i = 0; i + 1 < segments.size(); ++i) {
            TomlValue* existing = parent->find(segments[i]);
            if (existing == nullptr) {
                parent->set(segments[i], TomlValue::make_table());
                existing = parent->find(segments[i]);
            }
            else if (!existing->is_table()) {
                fail(seg_locs[i], "dotted key segment conflicts with non-table");
                path_ok = false;
                break;
            }
            parent = existing;
        }
        if (!path_ok)
            break;
        if (parent->find(segments.back()) != nullptr) {
            fail(seg_locs.back(), "duplicate key in inline table");
            break;
        }
        parent->set(segments.back(), std::move(val));

        skip_inline_ws();
        if (failed_)
            break;
        const u8 c = peek();
        if (c == ',') {
            advance();
            skip_inline_ws();
            if (failed_)
                break;
            continue;
        }
        if (c == '}') {
            advance();
            break;
        }
        fail(loc_, "expected ',' or '}' in inline table");
        break;
    }
    --depth_;
    return !failed_;
}

// ============================================================================
// Number / Datetime 解析
// ============================================================================

// 识别 datetime / number 字面量边界。
// TOML 中无引号值的有效字符：数字、字母、+、-、.、:、T、Z、_（分隔）。
// 扫描直到遇到空白/换行/注释/#/=/]/}/,。
// 例外：date-time 字面量允许在日期与时间之间用单个空格替代 T，如
//   1979-05-27 07:32:00
// 故当扫描前缀识别为日期（YYYY-MM-DD）时，允许吃掉紧随其后的一个空格并继续。
bool TomlParser::parse_number_or_datetime_value(TomlValue& out)
{
    SourceLocation start     = loc_;
    const u8*      base      = data_ + pos_;
    const usize    remaining = byte_length_ - pos_;
    auto           is_digit  = [](u8 c) { return c >= '0' && c <= '9'; };
    // 是否以日期前缀开头：YYYY-MM-DD
    bool looks_like_date = remaining >= 10 && is_digit(base[0]) && is_digit(base[1]) &&
                           is_digit(base[2]) && is_digit(base[3]) && base[4] == '-' &&
                           is_digit(base[5]) && is_digit(base[6]) && base[7] == '-' &&
                           is_digit(base[8]) && is_digit(base[9]);

    usize scan = pos_;
    while (scan < byte_length_) {
        const u8 c = data_[scan];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#' || c == ',' || c == ']' ||
            c == '}' || c == '=') {
            // 例外：date-time 日期与时间之间允许单个空格。
            // 仅在 looks_like_date 且当前正好在日期末尾（10 字符）且后续是时间模式时才吃下。
            if (c == ' ' && looks_like_date && scan - pos_ == 10 && scan + 1 < byte_length_ &&
                scan + 9 <= byte_length_) {
                const u8* t = data_ + scan + 1;
                if (is_digit(t[0]) && is_digit(t[1]) && t[2] == ':' && is_digit(t[3]) &&
                    is_digit(t[4]) && t[5] == ':' && is_digit(t[6]) && is_digit(t[7])) {
                    ++scan;   // 吃掉空格，继续扫描时间部分
                    continue;
                }
            }
            break;
        }
        ++scan;
    }
    const usize len = scan - pos_;
    if (len == 0) {
        fail(start, "expected value");
        return false;
    }
    const u8* begin = &data_[pos_];

    // 1) 进制整数 0x/0o/0b（大小写不敏感）
    if (len >= 2 && begin[0] == '0') {
        const u8 p = begin[1];
        if (p == 'x' || p == 'X') {
            return parse_integer_radix(begin, len, 16, out);
        }
        if (p == 'o' || p == 'O') {
            return parse_integer_radix(begin, len, 8, out);
        }
        if (p == 'b' || p == 'B') {
            return parse_integer_radix(begin, len, 2, out);
        }
    }

    // 2) datetime 探测：YYYY-MM-DD 或 HH:MM:SS 模式。
    TomlDatetime dt;
    usize        consumed_kind = 0;
    if (try_parse_datetime(begin, len, dt, consumed_kind)) {
        // 全部字面量都应被 datetime 消费。
        if (consumed_kind != len) {
            fail(start, "invalid datetime literal: trailing characters");
            return false;
        }
        switch (dt.kind) {
        case TomlDatetimeKind::OffsetDatetime: out = TomlValue::make_offset_datetime(dt); break;
        case TomlDatetimeKind::LocalDateTime: out = TomlValue::make_local_datetime(dt); break;
        case TomlDatetimeKind::LocalDate: out = TomlValue::make_local_date(dt); break;
        case TomlDatetimeKind::LocalTime: out = TomlValue::make_local_time(dt); break;
        }
        for (usize i = 0; i < consumed_kind; ++i)
            advance();
        return !failed_;
    }

    // 3) 十进制 number（int/float）。
    return parse_number_decimal(begin, len, out);
}

bool TomlParser::try_parse_datetime(const u8* p, usize len, TomlDatetime& out, usize& consumed)
{
    // 4 种格式（按优先级尝试）：
    //   a. offset date-time: YYYY-MM-DD[T<sep>]HH:MM:SS[.frac](Z|±HH:MM)
    //   b. local date-time : YYYY-MM-DD[T<sep>]HH:MM:SS[.frac]
    //   c. local date      : YYYY-MM-DD
    //   d. local time      : HH:MM:SS[.frac]
    // 日期要求 4 位年 - 2 月 - 2 日；时间 2:2:2[.frac]。
    auto is_digit = [](u8 c) { return c >= '0' && c <= '9'; };

    usize i = 0;
    // 日期候选：4 位数字后跟 '-'
    if (len >= 10 && is_digit(p[0]) && is_digit(p[1]) && is_digit(p[2]) && is_digit(p[3]) &&
        p[4] == '-' && is_digit(p[5]) && is_digit(p[6]) && p[7] == '-' && is_digit(p[8]) &&
        is_digit(p[9])) {
        // local date 至少匹配前 10 个字符。
        ca::i32 year  = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
        ca::u8  month = static_cast<ca::u8>((p[5] - '0') * 10 + (p[6] - '0'));
        ca::u8  day   = static_cast<ca::u8>((p[8] - '0') * 10 + (p[9] - '0'));
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            consumed = 0;
            return false;
        }
        i = 10;
        if (len == 10) {
            // 仅 local date
            out.kind  = TomlDatetimeKind::LocalDate;
            out.year  = year;
            out.month = month;
            out.day   = day;
            consumed  = 10;
            return true;
        }
        // 分隔符：T / t / 空格
        if (p[i] == 'T' || p[i] == 't' || p[i] == ' ') {
            ++i;
            // 期望时间 HH:MM:SS
            if (i + 8 > len) {
                consumed = 0;
                return false;
            }
            if (!(is_digit(p[i]) && is_digit(p[i + 1]) && p[i + 2] == ':' && is_digit(p[i + 3]) &&
                  is_digit(p[i + 4]) && p[i + 5] == ':' && is_digit(p[i + 6]) &&
                  is_digit(p[i + 7]))) {
                consumed = 0;
                return false;
            }
            ca::u8 hour   = static_cast<ca::u8>((p[i] - '0') * 10 + (p[i + 1] - '0'));
            ca::u8 minute = static_cast<ca::u8>((p[i + 3] - '0') * 10 + (p[i + 4] - '0'));
            ca::u8 second = static_cast<ca::u8>((p[i + 6] - '0') * 10 + (p[i + 7] - '0'));
            if (hour > 23 || minute > 59 || second > 60) {
                consumed = 0;
                return false;
            }
            i += 8;
            // 可选小数秒（最多取 9 位 → 纳秒，超出部分截断丢弃，不四舍五入）
            ca::u32 nanos = 0;
            if (i < len && p[i] == '.') {
                ++i;
                usize frac_start = i;
                // 先数小数位数
                usize frac_end = i;
                while (frac_end < len && is_digit(p[frac_end]))
                    ++frac_end;
                if (frac_end == frac_start) {
                    consumed = 0;
                    return false;
                }   // '.' 后无数字
                usize   ndigits = frac_end - frac_start;
                usize   use     = ndigits > 9 ? 9 : ndigits;
                ca::u32 frac    = 0;
                for (usize k = 0; k < use; ++k) {
                    frac = frac * 10 + static_cast<ca::u32>(p[frac_start + k] - '0');
                }
                // 补零到 9 位
                for (usize k = use; k < 9; ++k)
                    frac *= 10;
                nanos = frac;
                i     = frac_end;   // 跳过所有小数位（含被截断的）
            }
            out.year   = year;
            out.month  = month;
            out.day    = day;
            out.hour   = hour;
            out.minute = minute;
            out.second = second;
            out.nanos  = nanos;
            // 可选时区
            if (i < len) {
                if (p[i] == 'Z' || p[i] == 'z') {
                    ++i;
                    out.has_tz     = true;
                    out.tz_minutes = 0;
                    out.kind       = TomlDatetimeKind::OffsetDatetime;
                }
                else if (p[i] == '+' || p[i] == '-') {
                    const u8 sign = p[i];
                    ++i;
                    if (i + 5 > len || !is_digit(p[i]) || !is_digit(p[i + 1]) || p[i + 2] != ':' ||
                        !is_digit(p[i + 3]) || !is_digit(p[i + 4])) {
                        consumed = 0;
                        return false;
                    }
                    ca::u8 tz_h = static_cast<ca::u8>((p[i] - '0') * 10 + (p[i + 1] - '0'));
                    ca::u8 tz_m = static_cast<ca::u8>((p[i + 3] - '0') * 10 + (p[i + 4] - '0'));
                    if (tz_h > 23 || tz_m > 59) {
                        consumed = 0;
                        return false;
                    }
                    i += 5;
                    ca::i16 off = static_cast<ca::i16>(tz_h * 60 + tz_m);
                    if (sign == '-')
                        off = static_cast<ca::i16>(-off);
                    out.has_tz     = true;
                    out.tz_minutes = off;
                    out.kind       = TomlDatetimeKind::OffsetDatetime;
                }
                else {
                    // 无时区：local date-time
                    out.has_tz = false;
                    out.kind   = TomlDatetimeKind::LocalDateTime;
                }
            }
            else {
                out.has_tz = false;
                out.kind   = TomlDatetimeKind::LocalDateTime;
            }
            consumed = i;
            return true;
        }
        // 10 位后跟非分隔符：可能是 date 后面接了非法字符；交给调用方判断。
        // 仅在完全匹配 10 位时返回 local date；其它情况让 number 路径处理。
        // 这里直接返回 local date（consumed=10），调用方检查 consumed==len。
        out.kind  = TomlDatetimeKind::LocalDate;
        out.year  = year;
        out.month = month;
        out.day   = day;
        consumed  = 10;
        return true;
    }
    // 仅时间：HH:MM:SS[.frac]
    if (len >= 8 && is_digit(p[0]) && is_digit(p[1]) && p[2] == ':' && is_digit(p[3]) &&
        is_digit(p[4]) && p[5] == ':' && is_digit(p[6]) && is_digit(p[7])) {
        ca::u8 hour   = static_cast<ca::u8>((p[0] - '0') * 10 + (p[1] - '0'));
        ca::u8 minute = static_cast<ca::u8>((p[3] - '0') * 10 + (p[4] - '0'));
        ca::u8 second = static_cast<ca::u8>((p[6] - '0') * 10 + (p[7] - '0'));
        if (hour > 23 || minute > 59 || second > 60) {
            consumed = 0;
            return false;
        }
        usize   i     = 8;
        ca::u32 nanos = 0;
        if (i < len && p[i] == '.') {
            ++i;
            usize frac_start = i;
            usize frac_end   = i;
            while (frac_end < len && is_digit(p[frac_end]))
                ++frac_end;
            if (frac_end == frac_start) {
                consumed = 0;
                return false;
            }
            usize   ndigits = frac_end - frac_start;
            usize   use     = ndigits > 9 ? 9 : ndigits;
            ca::u32 frac    = 0;
            for (usize k = 0; k < use; ++k) {
                frac = frac * 10 + static_cast<ca::u32>(p[frac_start + k] - '0');
            }
            for (usize k = use; k < 9; ++k)
                frac *= 10;
            nanos = frac;
            i     = frac_end;
        }
        out.kind   = TomlDatetimeKind::LocalTime;
        out.hour   = hour;
        out.minute = minute;
        out.second = second;
        out.nanos  = nanos;
        out.has_tz = false;
        consumed   = i;
        return true;
    }
    consumed = 0;
    return false;
}

bool TomlParser::parse_number_decimal(const u8* begin, usize len, TomlValue& out)
{
    SourceLocation start = loc_;
    // 校验：去除下划线后调用 strtoll/strtod。
    // TOML 下划线规则：只能在数字之间；不在首尾；不在符号后；不在 . 或 e/E 前后。
    // 这里做宽松校验（不允许多个连续下划线、首尾下划线）。
    if (len == 0) {
        fail(start, "invalid number");
        return false;
    }

    // 前导零校验：整数部分（可选符号后）若首位是 '0'，则该位必须是整数部分的唯一数字。
    // 即 "0" 合法，"01" / "00.1" 非法。TOML 不允许前导零。
    {
        usize idx = 0;
        if (idx < len && (begin[idx] == '+' || begin[idx] == '-'))
            ++idx;
        if (idx < len && begin[idx] == '0') {
            usize next = idx + 1;
            if (next < len) {
                const u8 nc = begin[next];
                if (nc != '.' && nc != 'e' && nc != 'E' && nc != '_') {
                    fail(start, "leading zeros not allowed in number");
                    return false;
                }
            }
        }
    }

    std::string buf;
    buf.reserve(len);
    bool is_float = false;
    bool saw_exp  = false;
    bool saw_dot  = false;
    for (usize i = 0; i < len; ++i) {
        const u8 c = begin[i];
        if (c == '_') {
            // TOML 1.0：下划线必须夹在两个数字之间（e/E 与小数点都不算数字）
            if (i == 0 || i + 1 == len) {
                fail(start, "invalid underscore in number");
                return false;
            }
            const u8 prev = begin[i - 1];
            const u8 next = begin[i + 1];
            if (!(prev >= '0' && prev <= '9' && next >= '0' && next <= '9')) {
                fail(start, "invalid underscore placement in number");
                return false;
            }
            continue;   // 跳过下划线
        }
        if (c == '.') {
            if (saw_dot || saw_exp) {
                fail(start, "invalid number: multiple dots");
                return false;
            }
            // TOML ABNF：decfloat = dec-int frac，小数点两侧都必须是数字。
            // strtod 宽容接受 "1." / "+1." / "1.e2" / "-.5"，这里提前拒绝。
            const bool prev_is_digit = i > 0 && begin[i - 1] >= '0' && begin[i - 1] <= '9';
            const bool next_is_digit = i + 1 < len && begin[i + 1] >= '0' && begin[i + 1] <= '9';
            if (!prev_is_digit || !next_is_digit) {
                fail(start, "invalid float: '.' must be surrounded by digits");
                return false;
            }
            saw_dot  = true;
            is_float = true;
        }
        else if (c == 'e' || c == 'E') {
            if (saw_exp) {
                fail(start, "invalid number: multiple exponents");
                return false;
            }
            saw_exp  = true;
            is_float = true;
            // 后面可跟 +/-
            if (i + 1 < len && (begin[i + 1] == '+' || begin[i + 1] == '-')) {
                buf.push_back(static_cast<char>(c));
                buf.push_back(static_cast<char>(begin[i + 1]));
                ++i;
                continue;
            }
        }
        buf.push_back(static_cast<char>(c));
    }
    if (buf.empty()) {
        fail(start, "invalid number");
        return false;
    }
    buf.push_back('\0');

    char* endp = nullptr;
    errno      = 0;
    if (is_float) {
        double d = std::strtod(buf.data(), &endp);
        if (errno == ERANGE) {
            fail(start, "float out of range");
            return false;
        }
        if (endp != buf.data() + buf.size() - 1) {
            fail(start, "invalid float");
            return false;
        }
        out = TomlValue::make_float(d);
    }
    else {
        long long v = std::strtoll(buf.data(), &endp, 10);
        if (errno == ERANGE) {
            fail(start, "integer out of range (i64)");
            return false;
        }
        if (endp != buf.data() + buf.size() - 1) {
            fail(start, "invalid integer");
            return false;
        }
        out = TomlValue::make_integer(static_cast<ca::i64>(v));
    }
    // 推进游标
    for (usize i = 0; i < len; ++i)
        advance();
    return !failed_;
}

bool TomlParser::parse_integer_radix(const u8* begin, usize len, int radix, TomlValue& out)
{
    SourceLocation start = loc_;
    // begin[0..1] = "0x"/"0o"/"0b"
    // 允许下划线分隔（TOML 1.0：进制整数允许下划线，规则同十进制）。
    std::string buf;
    buf.reserve(len);
    for (usize i = 2; i < len; ++i) {
        const u8 c = begin[i];
        if (c == '_') {
            if (i == 2 || i + 1 == len) {
                fail(start, "invalid underscore in integer");
                return false;
            }
            const u8 prev = begin[i - 1];
            const u8 next = begin[i + 1];
            // 进制整数下划线必须在两位数字之间
            bool ok = (prev >= '0' && prev <= '9') ||
                      (radix == 16 && ((prev | 0x20) >= 'a' && (prev | 0x20) <= 'f'));
            ok = ok && ((next >= '0' && next <= '9') ||
                        (radix == 16 && ((next | 0x20) >= 'a' && (next | 0x20) <= 'f')));
            if (!ok) {
                fail(start, "invalid underscore placement in integer");
                return false;
            }
            continue;
        }
        buf.push_back(static_cast<char>(c));
    }
    if (buf.empty()) {
        fail(start, "invalid integer literal");
        return false;
    }
    // 字符集校验：strtoll 对任意进制都会消费可选符号与前导空白，"0x-5"/"0x 5"
    // 曾被静默解析成带符号值。进制整数的 ABNF 不允许符号，逐字符白名单拒绝。
    for (const char ch : buf) {
        const bool digit     = ch >= '0' && ch <= '9';
        const bool hex_alpha = radix == 16 && ((ch | 0x20) >= 'a' && (ch | 0x20) <= 'f');
        // 八进制/二进制的数字集是十进制子集，越界数字由下方 endp 全量消费校验兜底
        if (!digit && !hex_alpha) {
            fail(start, "invalid character in radix integer");
            return false;
        }
    }
    buf.push_back('\0');

    char* endp  = nullptr;
    errno       = 0;
    long long v = std::strtoll(buf.data(), &endp, radix);
    if (errno == ERANGE) {
        fail(start, "integer out of range (i64)");
        return false;
    }
    if (endp != buf.data() + buf.size() - 1) {
        fail(start, "invalid integer literal");
        return false;
    }
    out = TomlValue::make_integer(static_cast<ca::i64>(v));
    for (usize i = 0; i < len; ++i)
        advance();
    return !failed_;
}

bool TomlParser::parse_special_float(const u8* begin, usize len, TomlValue& out)
{
    // begin 指向字面量起点；len 为字符数（含符号）。
    // 大小写不敏感的 inf/nan，可带 +/- 前缀。
    SourceLocation start    = loc_;
    bool           negative = false;
    usize          i        = 0;
    if (begin[0] == '+') {
        i = 1;
    }
    else if (begin[0] == '-') {
        i        = 1;
        negative = true;
    }
    // 剩余应为 "inf" 或 "nan"
    usize rest = len - i;
    if (rest == 3) {
        if ((begin[i] | 0x20) == 'i' && (begin[i + 1] | 0x20) == 'n' &&
            (begin[i + 2] | 0x20) == 'f') {
            double v = negative ? -std::numeric_limits<double>::infinity()
                                : std::numeric_limits<double>::infinity();
            out      = TomlValue::make_float(v);
            for (usize k = 0; k < len; ++k)
                advance();
            return true;
        }
        if ((begin[i] | 0x20) == 'n' && (begin[i + 1] | 0x20) == 'a' &&
            (begin[i + 2] | 0x20) == 'n') {
            double v = std::numeric_limits<double>::quiet_NaN();
            out      = TomlValue::make_float(v);
            for (usize k = 0; k < len; ++k)
                advance();
            return true;
        }
    }
    fail(start, "invalid special float literal");
    return false;
}

// ============================================================================
// Hex 解码
// ============================================================================

bool TomlParser::parse_hex4(const u8* p, u32& out)
{
    u32 v = 0;
    for (int i = 0; i < 4; ++i) {
        u8  c = p[i];
        u32 d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return false;
        v = (v << 4) | d;
    }
    out = v;
    return true;
}

bool TomlParser::parse_hex8(const u8* p, u32& out)
{
    u32 v = 0;
    for (int i = 0; i < 8; ++i) {
        u8  c = p[i];
        u32 d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return false;
        v = (v << 4) | d;
        // 防止溢出：超过 0x10FFFF 是非法码点
        if (v > 0x7FFFFFFFu)
            return false;
    }
    out = v;
    return true;
}

bool TomlParser::decode_utf8(const u8* data, usize len, usize& i, u32& cp)
{
    if (i >= len)
        return false;
    const u8 c     = data[i];
    u32      extra = 0;
    if ((c & 0x80) == 0) {
        cp    = c;
        extra = 0;
    }
    else if ((c & 0xE0) == 0xC0) {
        cp    = c & 0x1F;
        extra = 1;
    }
    else if ((c & 0xF0) == 0xE0) {
        cp    = c & 0x0F;
        extra = 2;
    }
    else if ((c & 0xF8) == 0xF0) {
        cp    = c & 0x07;
        extra = 3;
    }
    else
        return false;
    if (i + extra >= len)
        return false;
    for (u32 k = 1; k <= extra; ++k) {
        const u8 cc = data[i + k];
        if ((cc & 0xC0) != 0x80)
            return false;
        cp = (cp << 6) | (cc & 0x3F);
    }
    i += extra;
    return true;
}

}   // namespace ca::toml
