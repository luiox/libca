#include "libca/yaml/yaml_parser.hpp"

#include "libca/yaml/yaml_scalar.hpp"

#include <utility>

namespace ca::yaml {

namespace {

/// 递归深度守卫：构造 ++，析构 --，防各 return 路径漏减。
struct DepthGuard {
    usize& depth;
    explicit DepthGuard(usize& d) noexcept : depth(d) { ++depth; }
    ~DepthGuard() { --depth; }
};

bool is_inline_ws(u8 c) { return c == ' ' || c == '\t'; }

}  // namespace

// ============================================================================
// 构造与入口
// ============================================================================

YamlParser::YamlParser(YamlDocument& document, const ca::str::Utf8StringRef& input,
                       const YamlParserOptions& options)
    : data_(input.data()),
      byte_length_(input.byte_length()),
      document_(document),
      options_(options) {
    if (data_ == nullptr) {
        data_ = reinterpret_cast<const u8*>("");
        byte_length_ = 0;
    }
}

const ParseError& YamlParser::last_error() const noexcept { return error_; }

bool YamlParser::run() {
    split_lines();
    if (failed_) return false;

    li_ = 0;
    col_ = lines_.empty() ? 0 : lines_[0].indent;

    if (!advance_to_content()) {
        // 空文档 / 只有空白与注释：root 保持 Null。
        return true;
    }

    // 开头允许单个 "---" 文档开始标记（同一行不允许再跟内容）。
    if (at_document_marker()) {
        const Line& L = lines_[li_];
        if (L.text[0] == '.') {
            fail(loc_here(), "multi-document YAML is not supported");
            return false;
        }
        col_ = 3;
        skip_inline_ws();
        if (col_ < L.length && L.text[col_] != '#') {
            fail(loc_here(), "content after '---' on the same line is not supported");
            return false;
        }
        finish_line();
    }

    YamlValue root;
    parse_block_node(0, root);
    if (failed_) return false;

    if (advance_to_content()) {
        if (at_document_marker()) {
            fail(loc_here(), "multi-document YAML is not supported");
        } else {
            fail(loc_here(), "unexpected content after document root");
        }
        return false;
    }

    document_.root() = std::move(root);
    return !failed_;
}

// ============================================================================
// 行预扫描
// ============================================================================

void YamlParser::split_lines() {
    usize pos = 0;
    // UTF-8 BOM
    if (byte_length_ >= 3 && data_[0] == 0xEF && data_[1] == 0xBB && data_[2] == 0xBF) {
        pos = 3;
    }
    usize line_no = 1;
    while (pos < byte_length_) {
        const usize start = pos;
        usize end = start;
        while (end < byte_length_ && data_[end] != '\n' && data_[end] != '\r') ++end;

        usize next;
        if (end == byte_length_) {
            next = end;  // 最后一行无换行
        } else if (data_[end] == '\n') {
            next = end + 1;
        } else {  // '\r'
            if (end + 1 < byte_length_ && data_[end + 1] == '\n') {
                next = end + 2;
            } else {
                SourceLocation loc;
                loc.offset = end;
                loc.line = line_no;
                loc.column = end - start + 1;
                fail(loc, "stray carriage return (only \\n and \\r\\n line endings are supported)");
                return;
            }
        }

        Line ln;
        ln.offset = start;
        ln.line_no = line_no++;
        ln.text = data_ + start;
        ln.length = end - start;

        usize ws = 0;
        bool tab_in_indent = false;
        while (ws < ln.length && is_inline_ws(ln.text[ws])) {
            if (ln.text[ws] == '\t') tab_in_indent = true;
            ++ws;
        }
        ln.blank = (ws == ln.length);
        if (!ln.blank && tab_in_indent) {
            SourceLocation loc;
            loc.offset = start;
            loc.line = ln.line_no;
            loc.column = 1;
            fail(loc, "tab character in indentation (use spaces)");
            return;
        }
        ln.indent = ws;
        ln.comment_only = !ln.blank && ln.text[ws] == '#';
        lines_.push_back(ln);

        pos = next;
    }
}

// ============================================================================
// 游标
// ============================================================================

bool YamlParser::advance_to_content() {
    while (li_ < lines_.size()) {
        const Line& L = lines_[li_];
        if (!L.blank && !L.comment_only) {
            usize c = col_;
            while (c < L.length && is_inline_ws(L.text[c])) ++c;
            if (c < L.length && L.text[c] != '#') {
                col_ = c;
                return true;
            }
        }
        ++li_;
        col_ = (li_ < lines_.size()) ? lines_[li_].indent : 0;
    }
    return false;
}

void YamlParser::finish_line() noexcept {
    ++li_;
    col_ = (li_ < lines_.size()) ? lines_[li_].indent : 0;
}

SourceLocation YamlParser::loc_here() const noexcept {
    SourceLocation loc;
    if (li_ < lines_.size()) {
        loc.offset = lines_[li_].offset + col_;
        loc.line = lines_[li_].line_no;
        loc.column = col_ + 1;
    } else {
        loc.offset = byte_length_;
        loc.line = lines_.empty() ? 1 : lines_.back().line_no;
        loc.column = 1;
    }
    return loc;
}

usize YamlParser::rest_length() const noexcept {
    if (li_ >= lines_.size()) return 0;
    const Line& L = lines_[li_];
    return col_ < L.length ? L.length - col_ : 0;
}

void YamlParser::skip_inline_ws() noexcept {
    if (li_ >= lines_.size()) return;
    const Line& L = lines_[li_];
    while (col_ < L.length && is_inline_ws(L.text[col_])) ++col_;
}

void YamlParser::fail(SourceLocation loc, const char* message) {
    if (failed_) return;
    failed_ = true;
    error_.location = loc;
    error_.message = ca::str::Utf8String::from_cstr(message);
}

// ============================================================================
// 判定
// ============================================================================

bool YamlParser::at_sequence_dash() const noexcept {
    if (li_ >= lines_.size()) return false;
    const Line& L = lines_[li_];
    if (col_ >= L.length || L.text[col_] != '-') return false;
    return col_ + 1 >= L.length || is_inline_ws(L.text[col_ + 1]);
}

bool YamlParser::at_document_marker() const noexcept {
    if (li_ >= lines_.size()) return false;
    const Line& L = lines_[li_];
    if (L.indent != 0 || col_ != 0 || L.length < 3) return false;
    const u8 c = L.text[0];
    if (c != '-' && c != '.') return false;
    if (L.text[1] != c || L.text[2] != c) return false;
    return L.length == 3 || is_inline_ws(L.text[3]);
}

bool YamlParser::line_has_mapping_key() const noexcept {
    const Line& L = lines_[li_];
    usize i = col_;
    if (i >= L.length) return false;
    const u8 first = L.text[i];
    // flow 集合开头的行按标量路径解析（复杂键不支持）。
    if (first == '[' || first == '{') return false;
    if (first == '\'' || first == '"') {
        // 引号 key：粗扫到闭引号（不分配），其后跳空白须紧跟 ": "。
        const u8 q = first;
        ++i;
        bool closed = false;
        while (i < L.length) {
            if (q == '\'') {
                if (L.text[i] == '\'') {
                    if (i + 1 < L.length && L.text[i + 1] == '\'') { i += 2; continue; }
                    ++i;
                    closed = true;
                    break;
                }
                ++i;
            } else {
                if (L.text[i] == '\\') { i += 2; continue; }
                if (L.text[i] == '"') { ++i; closed = true; break; }
                ++i;
            }
        }
        if (!closed) return false;  // 未闭合，交给标量路径报错
        while (i < L.length && is_inline_ws(L.text[i])) ++i;
        return i < L.length && L.text[i] == ':' &&
               (i + 1 >= L.length || is_inline_ws(L.text[i + 1]));
    }
    // plain key：首个后跟空白/行尾的 ':' 即分隔符；先遇尾注释则不是 key 行。
    for (; i < L.length; ++i) {
        const u8 c = L.text[i];
        if (c == '#' && i > col_ && is_inline_ws(L.text[i - 1])) return false;
        if (c == ':' && (i + 1 >= L.length || is_inline_ws(L.text[i + 1]))) return true;
    }
    return false;
}

bool YamlParser::reject_unsupported_indicator() {
    const Line& L = lines_[li_];
    if (col_ >= L.length) return false;
    switch (L.text[col_]) {
        case '&': fail(loc_here(), "anchors (&) are not supported"); return true;
        case '*': fail(loc_here(), "aliases (*) are not supported"); return true;
        case '!': fail(loc_here(), "tags (!) are not supported"); return true;
        case '%': fail(loc_here(), "YAML directives (%) are not supported"); return true;
        case '@':
        case '`': fail(loc_here(), "reserved indicator character"); return true;
        case '?':
            if (col_ + 1 >= L.length || is_inline_ws(L.text[col_ + 1])) {
                fail(loc_here(), "complex mapping keys ('? ') are not supported");
                return true;
            }
            return false;
        default: return false;
    }
}

// ============================================================================
// 块解析
// ============================================================================

void YamlParser::parse_block_node(usize min_indent, YamlValue& out) {
    DepthGuard guard(depth_);
    if (depth_ > options_.max_depth) {
        fail(loc_here(), "nesting too deep");
        return;
    }
    if (!advance_to_content() || cur_indent() < min_indent) {
        out = YamlValue::make_null();
        return;
    }
    if (at_document_marker()) {
        fail(loc_here(), "multi-document YAML is not supported");
        return;
    }
    if (at_sequence_dash()) {
        parse_block_sequence(cur_indent(), out);
        return;
    }
    const Line& L = lines_[li_];
    const u8 c = L.text[col_];
    if (c == '|' || c == '>') {
        parse_block_scalar(L.indent, out);
        return;
    }
    if (reject_unsupported_indicator()) return;
    if (line_has_mapping_key()) {
        parse_block_mapping(cur_indent(), out);
        return;
    }
    parse_scalar_node(out);
}

void YamlParser::parse_block_sequence(usize n, YamlValue& out) {
    out = YamlValue::make_sequence();
    for (;;) {
        if (failed_) return;
        if (!advance_to_content() || cur_indent() < n) return;
        if (at_document_marker()) {
            fail(loc_here(), "multi-document YAML is not supported");
            return;
        }
        if (cur_indent() > n) {
            fail(loc_here(), "invalid indentation in sequence");
            return;
        }
        if (!at_sequence_dash()) return;  // 同缩进非 '-'：交还调用方（如父 mapping 的下一个 key）

        ++col_;  // 消费 '-'
        const Line& L = lines_[li_];
        usize after = col_;
        while (after < L.length && is_inline_ws(L.text[after])) ++after;

        YamlValue item;
        if (after >= L.length || L.text[after] == '#') {
            // '-' 独占一行（或只跟注释）：项在后续更深缩进行，缺席则为 Null。
            finish_line();
            parse_block_node(n + 1, item);
        } else {
            col_ = after;
            parse_block_node(n + 1, item);
        }
        if (failed_) return;
        out.append(std::move(item));
    }
}

void YamlParser::parse_block_mapping(usize n, YamlValue& out) {
    out = YamlValue::make_mapping();
    for (;;) {
        if (failed_) return;
        if (!advance_to_content() || cur_indent() < n) return;
        if (at_document_marker()) {
            fail(loc_here(), "multi-document YAML is not supported");
            return;
        }
        if (cur_indent() > n) {
            fail(loc_here(),
                 "invalid indentation (multi-line plain scalars are not supported; use | or > "
                 "block scalars)");
            return;
        }
        if (at_sequence_dash()) {
            fail(loc_here(), "unexpected sequence entry '-' inside a mapping");
            return;
        }
        if (reject_unsupported_indicator()) return;

        ca::str::Utf8StringRef key;
        SourceLocation key_loc = loc_here();
        if (!parse_mapping_key(key, key_loc)) {
            if (!failed_) fail(key_loc, "expected ':' after mapping key");
            return;
        }
        if (out.find(key) != nullptr) {
            fail(key_loc, "duplicate mapping key");
            return;
        }
        YamlValue value;
        parse_mapping_value(n, value);
        if (failed_) return;
        out.set(key, std::move(value));
    }
}

bool YamlParser::parse_mapping_key(ca::str::Utf8StringRef& out_key, SourceLocation& key_loc) {
    const Line& L = lines_[li_];
    key_loc = loc_here();
    const u8 first = L.text[col_];

    if (first == '\'' || first == '"') {
        ca::str::Utf8StringBuilder sb;
        const bool ok = (first == '\'') ? parse_single_quoted(sb) : parse_double_quoted(sb);
        if (!ok) return false;
        skip_inline_ws();
        if (col_ >= L.length || L.text[col_] != ':') {
            fail(loc_here(), "expected ':' after quoted mapping key");
            return false;
        }
        ++col_;
        if (col_ < L.length && !is_inline_ws(L.text[col_])) {
            fail(loc_here(), "expected space after ':'");
            return false;
        }
        out_key = document_.arena().intern(sb.build_or_empty());
        return true;
    }

    // plain key：扫到分隔 ':'（后跟空白/行尾）。
    usize sep = col_;
    bool found = false;
    for (; sep < L.length; ++sep) {
        const u8 c = L.text[sep];
        if (c == '#' && sep > col_ && is_inline_ws(L.text[sep - 1])) break;
        if (c == ':' && (sep + 1 >= L.length || is_inline_ws(L.text[sep + 1]))) {
            found = true;
            break;
        }
    }
    if (!found) return false;

    usize key_end = sep;
    while (key_end > col_ && is_inline_ws(L.text[key_end - 1])) --key_end;
    if (key_end == col_) {
        fail(key_loc, "empty mapping key");
        return false;
    }
    out_key = document_.arena().intern(L.text + col_, key_end - col_);
    col_ = sep + 1;
    return true;
}

void YamlParser::parse_mapping_value(usize n, YamlValue& out) {
    skip_inline_ws();
    const Line& L = lines_[li_];
    if (col_ >= L.length || L.text[col_] == '#') {
        // 值不在本行：更深缩进 → 嵌套块；同缩进 '-' → 零缩进序列；否则 Null。
        finish_line();
        if (!advance_to_content()) {
            out = YamlValue::make_null();
            return;
        }
        if (cur_indent() > n) {
            parse_block_node(n + 1, out);
            return;
        }
        if (cur_indent() == n && at_sequence_dash()) {
            parse_block_sequence(n, out);
            return;
        }
        out = YamlValue::make_null();
        return;
    }
    const u8 c = L.text[col_];
    if (c == '|' || c == '>') {
        parse_block_scalar(n, out);
        return;
    }
    parse_scalar_node(out);
}

// ============================================================================
// 同行标量
// ============================================================================

void YamlParser::parse_scalar_node(YamlValue& out) {
    if (reject_unsupported_indicator()) return;
    const Line& L = lines_[li_];
    const u8 c = L.text[col_];
    if (c == '\'') {
        ca::str::Utf8StringBuilder sb;
        if (!parse_single_quoted(sb)) return;
        out = YamlValue::make_string(document_.arena().intern(sb.build_or_empty()));
        expect_line_end();
        return;
    }
    if (c == '"') {
        ca::str::Utf8StringBuilder sb;
        if (!parse_double_quoted(sb)) return;
        out = YamlValue::make_string(document_.arena().intern(sb.build_or_empty()));
        expect_line_end();
        return;
    }
    if (c == '[' || c == '{') {
        parse_flow_value(out);
        if (failed_) return;
        expect_line_end();
        return;
    }
    parse_plain_scalar(out);
}

void YamlParser::parse_plain_scalar(YamlValue& out) {
    const Line& L = lines_[li_];
    const usize start = col_;
    usize i = col_;
    for (; i < L.length; ++i) {
        const u8 c = L.text[i];
        if (c == '#' && i > start && is_inline_ws(L.text[i - 1])) break;  // 尾注释
        if (c == ':' && (i + 1 >= L.length || is_inline_ws(L.text[i + 1]))) {
            col_ = i;
            fail(loc_here(), "mapping value not allowed here; quote the scalar if ':' is intended");
            return;
        }
    }
    usize end = i;
    while (end > start && is_inline_ws(L.text[end - 1])) --end;

    const ResolvedScalar r = resolve_plain_scalar(L.text + start, end - start);
    switch (r.kind) {
        case PlainScalarKind::Null:    out = YamlValue::make_null(); break;
        case PlainScalarKind::Boolean: out = YamlValue::make_boolean(r.boolean); break;
        case PlainScalarKind::Integer: out = YamlValue::make_integer(r.integer); break;
        case PlainScalarKind::Float:   out = YamlValue::make_float(r.floating); break;
        case PlainScalarKind::String:
            out = YamlValue::make_string(document_.arena().intern(L.text + start, end - start));
            break;
        case PlainScalarKind::IntOverflow:
            fail(loc_here(), "integer out of range (i64)");
            return;
        case PlainScalarKind::FloatOverflow:
            fail(loc_here(), "float out of range");
            return;
    }
    finish_line();
}

void YamlParser::expect_line_end() {
    skip_inline_ws();
    const Line& L = lines_[li_];
    if (col_ < L.length && L.text[col_] != '#') {
        fail(loc_here(), "unexpected content after value");
        return;
    }
    finish_line();
}

// ============================================================================
// 引号字符串（单行）
// ============================================================================

bool YamlParser::parse_single_quoted(ca::str::Utf8StringBuilder& out) {
    const Line& L = lines_[li_];
    const SourceLocation start = loc_here();
    ++col_;  // 开引号
    while (col_ < L.length) {
        const u8 c = L.text[col_];
        if (c == '\'') {
            if (col_ + 1 < L.length && L.text[col_ + 1] == '\'') {
                out.append("'", 1);
                col_ += 2;
                continue;
            }
            ++col_;
            return true;
        }
        out.append(L.text + col_, 1);
        ++col_;
    }
    fail(start, "unterminated single-quoted string (multi-line quoted strings are not supported)");
    return false;
}

bool YamlParser::parse_double_quoted(ca::str::Utf8StringBuilder& out) {
    const Line& L = lines_[li_];
    const SourceLocation start = loc_here();
    ++col_;  // 开引号
    while (col_ < L.length) {
        const u8 c = L.text[col_];
        if (c == '"') {
            ++col_;
            return true;
        }
        if (c != '\\') {
            out.append(L.text + col_, 1);
            ++col_;
            continue;
        }
        // 转义
        ++col_;
        if (col_ >= L.length) break;
        const u8 e = L.text[col_];
        ++col_;
        switch (e) {
            case '0': out.append("\0", 1); break;
            case 'a': out.append("\a", 1); break;
            case 'b': out.append("\b", 1); break;
            case 't': out.append("\t", 1); break;
            case 'n': out.append("\n", 1); break;
            case 'v': out.append("\v", 1); break;
            case 'f': out.append("\f", 1); break;
            case 'r': out.append("\r", 1); break;
            case '"': out.append("\"", 1); break;
            case '/': out.append("/", 1); break;
            case '\\': out.append("\\", 1); break;
            case 'x': {
                if (col_ + 2 > L.length) { fail(start, "invalid \\x escape"); return false; }
                u8 byte = 0;
                for (int k = 0; k < 2; ++k) {
                    const u8 h = L.text[col_ + static_cast<usize>(k)];
                    u8 d;
                    if (h >= '0' && h <= '9') d = static_cast<u8>(h - '0');
                    else if (h >= 'a' && h <= 'f') d = static_cast<u8>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') d = static_cast<u8>(h - 'A' + 10);
                    else { fail(start, "invalid \\x escape"); return false; }
                    byte = static_cast<u8>((byte << 4) | d);
                }
                // YAML 1.2 的 \xXX 是码点转义（U+00XX）而非原始字节：XX >= 0x80
                // 需编码为两字节 UTF-8（此前按单字节追加产生非法序列，
                // 经 arena intern 静默变空串、字符串无声丢失）。
                // 与 \u 分支的 append_code_point 同路径。
                if (!out.append_code_point(byte)) {
                    fail(start, "invalid \\x escape");
                    return false;
                }
                col_ += 2;
                break;
            }
            case 'u': {
                if (col_ + 4 > L.length) { fail(start, "invalid \\u escape"); return false; }
                u32 cp;
                if (!parse_hex4(L.text + col_, cp)) { fail(start, "invalid \\u escape"); return false; }
                col_ += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    // 高代理：必须紧跟 \uDC00-\uDFFF 低代理。
                    if (col_ + 6 <= L.length && L.text[col_] == '\\' && L.text[col_ + 1] == 'u') {
                        u32 low;
                        if (!parse_hex4(L.text + col_ + 2, low)) {
                            fail(start, "invalid low surrogate");
                            return false;
                        }
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail(start, "invalid surrogate pair");
                            return false;
                        }
                        col_ += 6;
                        const u32 full = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        if (!out.append_code_point(full)) {
                            fail(start, "invalid code point");
                            return false;
                        }
                    } else {
                        fail(start, "unpaired high surrogate in \\u escape");
                        return false;
                    }
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail(start, "unpaired low surrogate in \\u escape");
                    return false;
                } else if (!out.append_code_point(cp)) {
                    fail(start, "invalid code point");
                    return false;
                }
                break;
            }
            case 'U': {
                if (col_ + 8 > L.length) { fail(start, "invalid \\U escape"); return false; }
                u32 cp;
                if (!parse_hex8(L.text + col_, cp)) { fail(start, "invalid \\U escape"); return false; }
                col_ += 8;
                if (!out.append_code_point(cp)) {
                    fail(start, "invalid code point");
                    return false;
                }
                break;
            }
            default:
                fail(start, "invalid escape sequence in double-quoted string");
                return false;
        }
    }
    fail(start, "unterminated double-quoted string (multi-line quoted strings are not supported)");
    return false;
}

// ============================================================================
// 块标量 | / >
// ============================================================================

void YamlParser::parse_block_scalar(usize n, YamlValue& out) {
    const Line& H = lines_[li_];
    const u8 style = H.text[col_];
    ++col_;
    int chomp = 0;  // -1 strip / 0 clip / +1 keep
    if (col_ < H.length && (H.text[col_] == '-' || H.text[col_] == '+')) {
        chomp = (H.text[col_] == '-') ? -1 : 1;
        ++col_;
    }
    if (col_ < H.length && H.text[col_] >= '1' && H.text[col_] <= '9') {
        fail(loc_here(), "explicit indent indicator on block scalar is not supported");
        return;
    }
    skip_inline_ws();
    if (col_ < H.length && H.text[col_] != '#') {
        fail(loc_here(), "unexpected content after block scalar indicator");
        return;
    }
    finish_line();

    // 定基准缩进：首个非空白行（缩进须 > n）。
    usize first = li_;
    while (first < lines_.size() && lines_[first].blank) ++first;
    if (first >= lines_.size() || lines_[first].indent <= n) {
        // 空内容（chomp keep 的尾空行简化为空串，见设计文档）。
        out = YamlValue::make_string(document_.arena().intern(
            ca::str::Utf8StringBuilder().build_or_empty()));
        return;
    }
    const usize m = lines_[first].indent;

    // 内容行片段：data 为 nullptr 表示空行。
    std::vector<std::pair<const u8*, usize>> content;
    for (usize i = li_; i < first; ++i) {
        // 前导空白行宽度不得超过基准缩进（YAML 规则，避免内容歧义）。
        if (lines_[i].indent > m) {
            li_ = i;
            col_ = 0;
            fail(loc_here(), "leading empty line more indented than block scalar");
            return;
        }
        content.push_back({nullptr, 0});
    }
    li_ = first;

    std::vector<usize> pending_blanks;  // 行下标，等看到后续内容才知道是"中间"还是"尾部"
    while (li_ < lines_.size()) {
        const Line& L = lines_[li_];
        if (L.blank) {
            pending_blanks.push_back(li_);
            ++li_;
            continue;
        }
        if (L.indent < m) break;
        for (usize idx : pending_blanks) {
            const Line& B = lines_[idx];
            // 空白行超过基准缩进的部分保留为内容（字面块中有效）。
            if (B.length > m) content.push_back({B.text + m, B.length - m});
            else content.push_back({nullptr, 0});
        }
        pending_blanks.clear();
        content.push_back({L.text + m, L.length - m});
        ++li_;
    }
    const usize trailing_blanks = pending_blanks.size();
    col_ = (li_ < lines_.size()) ? lines_[li_].indent : 0;

    ca::str::Utf8StringBuilder sb;
    if (style == '|') {
        // 字面：行间原样 '\n'。
        for (usize i = 0; i < content.size(); ++i) {
            if (i > 0) sb.append("\n", 1);
            if (content[i].first != nullptr) sb.append(content[i].first, content[i].second);
        }
    } else {
        // 折叠：相邻基准缩进内容行之间 → 单空格；k 个空行 → k 个 '\n'；
        // 更深缩进行保留字面换行（两侧都不折叠）。
        bool prev_content = false;
        bool prev_more_indented = false;
        usize blanks = 0;
        for (const auto& seg : content) {
            const bool empty = (seg.second == 0);
            if (empty) {
                ++blanks;
                continue;
            }
            const bool more = is_inline_ws(seg.first[0]);
            if (prev_content) {
                if (blanks > 0) {
                    for (usize k = 0; k < blanks; ++k) sb.append("\n", 1);
                } else {
                    sb.append((prev_more_indented || more) ? "\n" : " ", 1);
                }
            } else {
                for (usize k = 0; k < blanks; ++k) sb.append("\n", 1);
            }
            blanks = 0;
            sb.append(seg.first, seg.second);
            prev_content = true;
            prev_more_indented = more;
        }
    }
    // chomping
    if (chomp == 0) {
        sb.append("\n", 1);  // clip：恰好一个尾换行
    } else if (chomp == 1) {
        sb.append("\n", 1);
        for (usize k = 0; k < trailing_blanks; ++k) sb.append("\n", 1);
    }
    // strip：不加

    out = YamlValue::make_string(document_.arena().intern(sb.build_or_empty()));
}

// ============================================================================
// flow 集合（单行）
// ============================================================================

void YamlParser::parse_flow_value(YamlValue& out) {
    DepthGuard guard(depth_);
    if (depth_ > options_.max_depth) {
        fail(loc_here(), "nesting too deep");
        return;
    }
    skip_inline_ws();
    const Line& L = lines_[li_];
    if (col_ >= L.length) {
        fail(loc_here(), "flow collection must be closed on the same line");
        return;
    }
    const u8 c = L.text[col_];
    if (c == '[') { parse_flow_sequence(out); return; }
    if (c == '{') { parse_flow_mapping(out); return; }
    if (c == '\'') {
        ca::str::Utf8StringBuilder sb;
        if (!parse_single_quoted(sb)) return;
        out = YamlValue::make_string(document_.arena().intern(sb.build_or_empty()));
        return;
    }
    if (c == '"') {
        ca::str::Utf8StringBuilder sb;
        if (!parse_double_quoted(sb)) return;
        out = YamlValue::make_string(document_.arena().intern(sb.build_or_empty()));
        return;
    }
    if (reject_unsupported_indicator()) return;

    // flow plain：到 , ] } 或行尾；" #" 注释与 ": " 在 flow 内报错。
    const usize start = col_;
    while (col_ < L.length) {
        const u8 ch = L.text[col_];
        if (ch == ',' || ch == ']' || ch == '}') break;
        if (ch == '#' && col_ > start && is_inline_ws(L.text[col_ - 1])) {
            fail(loc_here(), "comment not allowed inside a flow collection");
            return;
        }
        if (ch == ':' && (col_ + 1 >= L.length || is_inline_ws(L.text[col_ + 1]))) {
            fail(loc_here(), "mapping entry not allowed here; quote the string or use { }");
            return;
        }
        ++col_;
    }
    usize end = col_;
    while (end > start && is_inline_ws(L.text[end - 1])) --end;
    if (end == start) {
        fail(loc_here(), "empty flow scalar");
        return;
    }
    const ResolvedScalar r = resolve_plain_scalar(L.text + start, end - start);
    switch (r.kind) {
        case PlainScalarKind::Null:    out = YamlValue::make_null(); break;
        case PlainScalarKind::Boolean: out = YamlValue::make_boolean(r.boolean); break;
        case PlainScalarKind::Integer: out = YamlValue::make_integer(r.integer); break;
        case PlainScalarKind::Float:   out = YamlValue::make_float(r.floating); break;
        case PlainScalarKind::String:
            out = YamlValue::make_string(document_.arena().intern(L.text + start, end - start));
            break;
        case PlainScalarKind::IntOverflow:  fail(loc_here(), "integer out of range (i64)"); return;
        case PlainScalarKind::FloatOverflow: fail(loc_here(), "float out of range"); return;
    }
}

void YamlParser::parse_flow_sequence(YamlValue& out) {
    ++col_;  // '['
    out = YamlValue::make_sequence();
    skip_inline_ws();
    {
        const Line& L = lines_[li_];
        if (col_ < L.length && L.text[col_] == ']') {
            ++col_;
            return;
        }
    }
    for (;;) {
        YamlValue item;
        parse_flow_value(item);
        if (failed_) return;
        out.append(std::move(item));
        skip_inline_ws();
        const Line& L = lines_[li_];
        if (col_ >= L.length) {
            fail(loc_here(), "flow collection must be closed on the same line");
            return;
        }
        const u8 ch = L.text[col_];
        if (ch == ',') {
            ++col_;
            skip_inline_ws();
            if (col_ < L.length && L.text[col_] == ']') {
                fail(loc_here(), "trailing comma not allowed in flow collection");
                return;
            }
            continue;
        }
        if (ch == ']') {
            ++col_;
            return;
        }
        fail(loc_here(), "expected ',' or ']' in flow sequence");
        return;
    }
}

void YamlParser::parse_flow_mapping(YamlValue& out) {
    ++col_;  // '{'
    out = YamlValue::make_mapping();
    skip_inline_ws();
    {
        const Line& L = lines_[li_];
        if (col_ < L.length && L.text[col_] == '}') {
            ++col_;
            return;
        }
    }
    for (;;) {
        skip_inline_ws();
        const Line& L = lines_[li_];
        if (col_ >= L.length) {
            fail(loc_here(), "flow collection must be closed on the same line");
            return;
        }
        // key
        ca::str::Utf8StringRef key;
        const SourceLocation key_loc = loc_here();
        const u8 kc = L.text[col_];
        if (kc == '\'' || kc == '"') {
            ca::str::Utf8StringBuilder sb;
            const bool ok = (kc == '\'') ? parse_single_quoted(sb) : parse_double_quoted(sb);
            if (!ok) return;
            key = document_.arena().intern(sb.build_or_empty());
            skip_inline_ws();
            if (col_ >= L.length || L.text[col_] != ':') {
                fail(loc_here(), "expected ':' in flow mapping");
                return;
            }
            ++col_;
        } else {
            // plain key：扫到后跟空白/,/} 的 ':'。
            usize sep = col_;
            bool found = false;
            for (; sep < L.length; ++sep) {
                const u8 c = L.text[sep];
                if (c == ',' || c == '}') break;
                if (c == ':' && (sep + 1 >= L.length || is_inline_ws(L.text[sep + 1]) ||
                                 L.text[sep + 1] == ',' || L.text[sep + 1] == '}')) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                fail(key_loc, "expected ':' in flow mapping");
                return;
            }
            usize key_end = sep;
            while (key_end > col_ && is_inline_ws(L.text[key_end - 1])) --key_end;
            if (key_end == col_) {
                fail(key_loc, "empty mapping key");
                return;
            }
            key = document_.arena().intern(L.text + col_, key_end - col_);
            col_ = sep + 1;
        }
        if (out.find(key) != nullptr) {
            fail(key_loc, "duplicate mapping key");
            return;
        }
        // value（':' 后紧跟 ',' 或 '}' 表示空值 Null）
        skip_inline_ws();
        YamlValue value;
        {
            const Line& L2 = lines_[li_];
            if (col_ < L2.length && (L2.text[col_] == ',' || L2.text[col_] == '}')) {
                value = YamlValue::make_null();
            } else {
                parse_flow_value(value);
                if (failed_) return;
            }
        }
        out.set(key, std::move(value));
        skip_inline_ws();
        const Line& L3 = lines_[li_];
        if (col_ >= L3.length) {
            fail(loc_here(), "flow collection must be closed on the same line");
            return;
        }
        const u8 ch = L3.text[col_];
        if (ch == ',') {
            ++col_;
            skip_inline_ws();
            if (col_ < L3.length && L3.text[col_] == '}') {
                fail(loc_here(), "trailing comma not allowed in flow collection");
                return;
            }
            continue;
        }
        if (ch == '}') {
            ++col_;
            return;
        }
        fail(loc_here(), "expected ',' or '}' in flow mapping");
        return;
    }
}

// ============================================================================
// 十六进制辅助
// ============================================================================

bool YamlParser::parse_hex4(const u8* p, u32& out) {
    if (p == nullptr) return false;
    u32 v = 0;
    for (int i = 0; i < 4; ++i) {
        const u8 c = p[i];
        u32 d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        v = (v << 4) | d;
    }
    out = v;
    return true;
}

bool YamlParser::parse_hex8(const u8* p, u32& out) {
    if (p == nullptr) return false;
    u32 v = 0;
    for (int i = 0; i < 8; ++i) {
        const u8 c = p[i];
        u32 d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        v = (v << 4) | d;
    }
    out = v;
    return true;
}

}  // namespace ca::yaml
