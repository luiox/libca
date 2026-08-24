#include "libca/xml/xml_parser.hpp"

#include "libca/str/format.hpp"
#include "libca/str/utf8_util.hpp"

#include <cstring>
#include <string>

namespace ca::xml {

namespace {

// 递归深度 RAII，析构自动回退，防止提前 return 漏减。
struct DepthGuard {
    ca::usize& depth;
    explicit DepthGuard(ca::usize& d) : depth(d) { ++depth; }
    ~DepthGuard() { --depth; }
};

bool all_whitespace(const u8* data, ca::usize len) noexcept {
    for (ca::usize i = 0; i < len; ++i) {
        const u8 c = data[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
    }
    return true;
}

}  // namespace

XmlParser::XmlParser(XmlDocument& document, const ca::str::Utf8StringRef& input,
                     const XmlParserOptions& options)
    : data_(input.data()),
      byte_length_(input.byte_length()),
      pos_(0),
      loc_(),
      failed_(false),
      depth_(0),
      document_(document),
      options_(options) {}

const ParseError& XmlParser::last_error() const noexcept { return error_; }

// ============================================================================
// 字节游标
// ============================================================================

u8 XmlParser::peek() const noexcept { return pos_ < byte_length_ ? data_[pos_] : 0; }

u8 XmlParser::peek_at(ca::usize offset) const noexcept {
    const ca::usize p = pos_ + offset;
    return p < byte_length_ ? data_[p] : 0;
}

void XmlParser::advance() noexcept {
    if (pos_ >= byte_length_) return;
    const u8 c = data_[pos_];
    ++pos_;
    ++loc_.offset;
    if (c == '\n') {
        ++loc_.line;
        loc_.column = 1;
    } else {
        ++loc_.column;
    }
}

bool XmlParser::at_end() const noexcept { return pos_ >= byte_length_; }

bool XmlParser::starts_with(const char* literal) const noexcept {
    const ca::usize n = std::strlen(literal);
    if (pos_ + n > byte_length_) return false;
    return std::memcmp(data_ + pos_, literal, n) == 0;
}

bool XmlParser::skip_ws() {
    bool any = false;
    while (!at_end() && is_ws(peek())) {
        advance();
        any = true;
    }
    return any;
}

void XmlParser::fail(SourceLocation loc, const char* message) {
    if (failed_) return;
    failed_ = true;
    error_.location = loc;
    error_.message = ca::str::Utf8String::from_cstr(message);
}

void XmlParser::fail_str(SourceLocation loc, const std::string& message) {
    if (failed_) return;
    failed_ = true;
    error_.location = loc;
    error_.message = ca::str::Utf8String::from_cstr(message.c_str());
}

// ============================================================================
// 字符判定 / 编码
// ============================================================================

bool XmlParser::is_ws(u8 c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool XmlParser::is_name_start(u8 c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':' || c >= 0x80;
}

bool XmlParser::is_name_char(u8 c) noexcept {
    return is_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

bool XmlParser::encode_utf8(u32 cp, ca::str::Utf8StringBuilder& out) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
    u8 buf[4];
    if (cp <= 0x7F) {
        buf[0] = static_cast<u8>(cp);
        out.append(buf, 1);
    } else if (cp <= 0x7FF) {
        buf[0] = static_cast<u8>(0xC0 | (cp >> 6));
        buf[1] = static_cast<u8>(0x80 | (cp & 0x3F));
        out.append(buf, 2);
    } else if (cp <= 0xFFFF) {
        buf[0] = static_cast<u8>(0xE0 | (cp >> 12));
        buf[1] = static_cast<u8>(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = static_cast<u8>(0x80 | (cp & 0x3F));
        out.append(buf, 3);
    } else {
        buf[0] = static_cast<u8>(0xF0 | (cp >> 18));
        buf[1] = static_cast<u8>(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = static_cast<u8>(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = static_cast<u8>(0x80 | (cp & 0x3F));
        out.append(buf, 4);
    }
    return true;
}

// ============================================================================
// 顶层
// ============================================================================

void XmlParser::skip_bom() {
    if (byte_length_ >= 3 && data_[0] == 0xEF && data_[1] == 0xBB && data_[2] == 0xBF) {
        advance();
        advance();
        advance();
    }
}

bool XmlParser::run() {
    skip_bom();

    // 整体校验 UTF-8：此前元素名/属性值/文本里的非法序列经 arena intern 静默变
    // 空串（产出空名元素的损坏 DOM），而注释/CDATA 走 intern_raw 原样保留——
    // 同一解析器两种行为。入口一次校验统一拒绝。
    if (!ca::str::utf8_is_valid(data_, byte_length_)) {
        fail(loc_, "invalid UTF-8 in XML document");
        return false;
    }

    // 可选 XML 声明（必须紧跟在 BOM/文件开头）。
    if (starts_with("<?xml") && (is_ws(peek_at(5)) || peek_at(5) == '?')) {
        if (!parse_declaration()) return false;
    }

    // prolog：空白 + 注释，遇根元素起始停。
    if (!parse_misc(document_.prolog())) return false;

    if (at_end()) {
        fail(loc_, "empty document: expected a root element");
        return false;
    }
    if (peek() != '<') {
        fail(loc_, "expected root element");
        return false;
    }

    // 根元素。
    XmlNode root;
    if (!parse_element(root)) return false;
    document_.root() = std::move(root);

    // epilog：空白 + 注释；再出现元素则多根报错。
    if (!parse_misc(document_.epilog())) return false;
    if (!at_end()) {
        fail(loc_, "only one root element allowed (trailing content after root)");
        return false;
    }
    return true;
}

bool XmlParser::parse_declaration() {
    // 已确认前缀 <?xml。消费它。
    for (int i = 0; i < 5; ++i) advance();

    document_.declaration().present = true;
    // 读伪属性直到 ?>。
    while (true) {
        skip_ws();
        if (at_end()) {
            fail(loc_, "unterminated XML declaration");
            return false;
        }
        if (peek() == '?') {
            advance();
            if (peek() != '>') {
                fail(loc_, "malformed XML declaration: expected '?>'");
                return false;
            }
            advance();
            return true;
        }
        // name = "value"
        const SourceLocation name_loc = loc_;
        ca::str::Utf8StringRef name;
        if (!parse_name(name)) return false;
        skip_ws();
        if (peek() != '=') {
            fail(name_loc, "malformed XML declaration attribute: expected '='");
            return false;
        }
        advance();
        skip_ws();
        ca::str::Utf8StringRef value;
        if (!parse_attr_value(value)) return false;

        const std::string_view key(reinterpret_cast<const char*>(name.data()), name.byte_length());
        if (key == "version") {
            document_.declaration().version = value;
        } else if (key == "encoding") {
            document_.declaration().encoding = value;
        } else if (key == "standalone") {
            document_.declaration().standalone = value;
        }
        // 未知伪属性宽松忽略（配置场景无需严格）。
    }
}

bool XmlParser::parse_misc(std::vector<XmlNode>& out) {
    while (true) {
        skip_ws();
        if (at_end()) return true;
        if (peek() != '<') return true;  // 元素起始或文本（顶层文本由调用方判定）

        if (starts_with("<!--")) {
            ca::str::Utf8StringRef comment;
            if (!parse_comment(comment)) return false;
            out.push_back(XmlNode::make_comment(comment));
            continue;
        }
        if (starts_with("<!DOCTYPE") || starts_with("<!doctype")) {
            fail(loc_, "DOCTYPE/DTD is not supported");
            return false;
        }
        if (starts_with("<?")) {
            fail(loc_, "processing instructions are not supported");
            return false;
        }
        // 其它 '<'（元素起始，或 epilog 中的第二个根）→ 交回调用方。
        return true;
    }
}

// ============================================================================
// 元素
// ============================================================================

bool XmlParser::parse_element(XmlNode& out) {
    DepthGuard guard(depth_);
    if (depth_ > options_.max_depth) {
        fail(loc_, "maximum element nesting depth exceeded");
        return false;
    }

    // 已在 '<'。
    const SourceLocation open_loc = loc_;
    advance();  // 消费 '<'
    if (!is_name_start(peek())) {
        fail(open_loc, "expected element name after '<'");
        return false;
    }
    ca::str::Utf8StringRef name;
    if (!parse_name(name)) return false;

    out = XmlNode::make_element(name);
    if (!parse_attributes(out)) return false;

    skip_ws();
    if (peek() == '/') {
        advance();
        if (peek() != '>') {
            fail(loc_, "malformed self-closing tag: expected '/>'");
            return false;
        }
        advance();
        return true;  // 自闭合，无子节点
    }
    if (peek() != '>') {
        fail(loc_, "expected '>' or '/>' in start tag");
        return false;
    }
    advance();  // 消费 '>'

    if (!parse_content(out)) return false;
    return parse_end_tag(name);
}

bool XmlParser::parse_name(ca::str::Utf8StringRef& out) {
    const ca::usize start = pos_;
    while (!at_end() && is_name_char(peek())) advance();
    if (pos_ == start) {
        fail(loc_, "expected a name");
        return false;
    }
    out = document_.arena().intern(data_ + start, pos_ - start);
    return true;
}

bool XmlParser::parse_attributes(XmlNode& element) {
    while (true) {
        const bool had_ws = skip_ws();
        const u8 c = peek();
        if (c == '>' || c == '/' || at_end()) return true;
        if (!had_ws) {
            fail(loc_, "expected whitespace before attribute");
            return false;
        }
        if (!is_name_start(c)) {
            fail(loc_, "expected attribute name");
            return false;
        }
        const SourceLocation name_loc = loc_;
        ca::str::Utf8StringRef name;
        if (!parse_name(name)) return false;
        skip_ws();
        if (peek() != '=') {
            fail(name_loc, "expected '=' after attribute name");
            return false;
        }
        advance();
        skip_ws();
        ca::str::Utf8StringRef value;
        if (!parse_attr_value(value)) return false;

        if (element.has_attribute(name)) {
            const std::string msg =
                ca::str::format_std("duplicate attribute '{}'", name);
            fail_str(name_loc, msg);
            return false;
        }
        element.set_attribute(name, value);
    }
}

bool XmlParser::parse_attr_value(ca::str::Utf8StringRef& out) {
    const u8 quote = peek();
    if (quote != '"' && quote != '\'') {
        fail(loc_, "expected quoted attribute value");
        return false;
    }
    advance();
    ca::str::Utf8StringBuilder sb;
    while (true) {
        if (at_end()) {
            fail(loc_, "unterminated attribute value");
            return false;
        }
        const u8 c = peek();
        if (c == quote) {
            advance();
            break;
        }
        if (c == '<') {
            fail(loc_, "'<' is not allowed in an attribute value");
            return false;
        }
        if (c == '&') {
            if (!parse_reference(sb)) return false;
            continue;
        }
        sb.append(&c, 1);
        advance();
    }
    out = document_.arena().intern(sb.build_or_empty());
    return true;
}

bool XmlParser::parse_content(XmlNode& element) {
    ca::str::Utf8StringBuilder text;
    bool has_text = false;

    auto flush_text = [&]() -> void {
        if (!has_text) return;
        ca::str::Utf8String built = text.build_or_empty();
        const bool drop = options_.trim_whitespace && all_whitespace(built.data(), built.byte_length());
        if (!drop) {
            element.append_child(XmlNode::make_text(document_.arena().intern(built)));
        }
        text = ca::str::Utf8StringBuilder();
        has_text = false;
    };

    while (true) {
        if (at_end()) {
            fail(loc_, "unexpected end of input: unclosed element");
            return false;
        }
        const u8 c = peek();
        if (c == '<') {
            if (starts_with("</")) {
                flush_text();
                return true;  // 结束标签，交回 parse_element
            }
            if (starts_with("<!--")) {
                flush_text();
                ca::str::Utf8StringRef comment;
                if (!parse_comment(comment)) return false;
                element.append_child(XmlNode::make_comment(comment));
                continue;
            }
            if (starts_with("<![CDATA[")) {
                flush_text();
                ca::str::Utf8StringRef cdata;
                if (!parse_cdata(cdata)) return false;
                element.append_child(XmlNode::make_cdata(cdata));
                continue;
            }
            if (starts_with("<!")) {
                fail(loc_, "DOCTYPE/DTD or declarations are not allowed here");
                return false;
            }
            if (starts_with("<?")) {
                fail(loc_, "processing instructions are not supported");
                return false;
            }
            // 子元素
            flush_text();
            XmlNode child;
            if (!parse_element(child)) return false;
            element.append_child(std::move(child));
            continue;
        }
        if (c == '&') {
            if (!parse_reference(text)) return false;
            has_text = true;
            continue;
        }
        text.append(&c, 1);
        has_text = true;
        advance();
    }
}

bool XmlParser::parse_end_tag(const ca::str::Utf8StringRef& expected_name) {
    // 已在 '</'。
    const SourceLocation loc = loc_;
    advance();  // '<'
    advance();  // '/'
    ca::str::Utf8StringRef name;
    if (!parse_name(name)) return false;
    skip_ws();
    if (peek() != '>') {
        fail(loc_, "expected '>' in closing tag");
        return false;
    }
    advance();
    if (name != expected_name) {
        const std::string msg = ca::str::format_std(
            "mismatched closing tag: expected </{}>, got </{}>", expected_name, name);
        fail_str(loc, msg);
        return false;
    }
    return true;
}

// ============================================================================
// 叶子
// ============================================================================

bool XmlParser::parse_comment(ca::str::Utf8StringRef& out) {
    // 已在 '<!--'。
    for (int i = 0; i < 4; ++i) advance();
    const ca::usize start = pos_;
    while (!at_end() && !starts_with("-->")) advance();
    if (at_end()) {
        fail(loc_, "unterminated comment");
        return false;
    }
    out = document_.arena().intern_raw(data_ + start, pos_ - start);
    for (int i = 0; i < 3; ++i) advance();  // 消费 -->
    return true;
}

bool XmlParser::parse_cdata(ca::str::Utf8StringRef& out) {
    // 已在 '<![CDATA['。
    for (int i = 0; i < 9; ++i) advance();
    const ca::usize start = pos_;
    while (!at_end() && !starts_with("]]>")) advance();
    if (at_end()) {
        fail(loc_, "unterminated CDATA section");
        return false;
    }
    out = document_.arena().intern_raw(data_ + start, pos_ - start);
    for (int i = 0; i < 3; ++i) advance();  // 消费 ]]>
    return true;
}

bool XmlParser::parse_reference(ca::str::Utf8StringBuilder& out) {
    const SourceLocation amp_loc = loc_;
    advance();  // '&'
    if (peek() == '#') {
        advance();
        u32 cp = 0;
        bool hex = false;
        if (peek() == 'x' || peek() == 'X') {
            hex = true;
            advance();
        }
        const ca::usize digit_start = pos_;
        while (!at_end() && peek() != ';') {
            const u8 d = peek();
            u32 v;
            if (d >= '0' && d <= '9') {
                v = static_cast<u32>(d - '0');
            } else if (hex && d >= 'a' && d <= 'f') {
                v = static_cast<u32>(d - 'a' + 10);
            } else if (hex && d >= 'A' && d <= 'F') {
                v = static_cast<u32>(d - 'A' + 10);
            } else {
                fail(amp_loc, "malformed numeric character reference");
                return false;
            }
            cp = cp * (hex ? 16u : 10u) + v;
            if (cp > 0x10FFFF) {
                fail(amp_loc, "character reference out of range");
                return false;
            }
            advance();
        }
        if (at_end() || pos_ == digit_start) {
            fail(amp_loc, "malformed numeric character reference");
            return false;
        }
        advance();  // ';'
        if (!encode_utf8(cp, out)) {
            fail(amp_loc, "character reference resolves to an invalid code point");
            return false;
        }
        return true;
    }

    // 命名实体。
    const ca::usize name_start = pos_;
    while (!at_end() && peek() != ';' && is_name_char(peek())) advance();
    if (at_end() || peek() != ';') {
        fail(amp_loc, "unterminated entity reference");
        return false;
    }
    const std::string_view ent(reinterpret_cast<const char*>(data_ + name_start), pos_ - name_start);
    advance();  // ';'
    char decoded = 0;
    if (ent == "lt") decoded = '<';
    else if (ent == "gt") decoded = '>';
    else if (ent == "amp") decoded = '&';
    else if (ent == "apos") decoded = '\'';
    else if (ent == "quot") decoded = '"';
    else {
        const std::string msg =
            ca::str::format_std("unknown entity reference '&{};' (custom entities/DTD not supported)",
                                ent);
        fail_str(amp_loc, msg);
        return false;
    }
    const u8 b = static_cast<u8>(decoded);
    out.append(&b, 1);
    return true;
}

}  // namespace ca::xml
