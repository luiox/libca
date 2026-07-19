#include "libca/json/json_parser.hpp"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace ca::json {

// ============================================================================
// 构造与游标
// ============================================================================

JsonParser::JsonParser(ca::str::Utf8StringRef input, JsonHandler& handler,
                       const JsonParserOptions& options)
    : data_(input.data()),
      byte_length_(input.byte_length()),
      pos_(0),
      loc_(),
      failed_(false),
      depth_(0),
      handler_(handler),
      options_(options),
      error_() {
    if (data_ == nullptr) {
        data_ = reinterpret_cast<const u8*>("");  // 空输入安全（pos 立即到 end）
        byte_length_ = 0;
    }
}

SourceLocation JsonParser::location() const noexcept { return loc_; }

const ParseError& JsonParser::last_error() const noexcept { return error_; }

u8 JsonParser::peek() const noexcept {
    return pos_ < byte_length_ ? data_[pos_] : 0;
}

u8 JsonParser::peek_next() const noexcept {
    return pos_ + 1 < byte_length_ ? data_[pos_ + 1] : 0;
}

bool JsonParser::at_end() const noexcept { return pos_ >= byte_length_; }

void JsonParser::advance() noexcept {
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

void JsonParser::fail(SourceLocation loc, const char* message) {
    if (failed_) return;  // 首错即停
    failed_ = true;
    error_.location = loc;
    error_.message = ca::str::Utf8String::from_cstr(message);
    handler_.on_error(error_);
}

// ============================================================================
// 空白与注释
// ============================================================================

void JsonParser::skip_ws_and_comments() {
    while (!failed_ && pos_ < byte_length_) {
        const u8 c = data_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        } else if (options_.allow_comments && c == '/') {
            if (peek_next() == '/') {
                // 行注释：到行尾
                advance();  // '/'
                advance();  // '/'
                while (pos_ < byte_length_ && data_[pos_] != '\n') advance();
            } else if (peek_next() == '*') {
                // 块注释：到 */
                SourceLocation start = loc_;
                advance();  // '/'
                advance();  // '*'
                bool closed = false;
                while (pos_ < byte_length_) {
                    if (data_[pos_] == '*' && peek_next() == '/') {
                        advance(); advance();
                        closed = true;
                        break;
                    }
                    advance();
                }
                if (!closed) {
                    fail(start, "unterminated block comment");
                    return;
                }
            } else {
                return;  // 不是注释，交给上层报错
            }
        } else {
            return;
        }
    }
}

// ============================================================================
// parse 入口
// ============================================================================

bool JsonParser::parse() {
    skip_ws_and_comments();
    if (failed_) return false;

    if (at_end()) {
        fail(loc_, "empty input");
        return false;
    }

    // BOM 拒绝
    if (byte_length_ >= 3 && data_[0] == 0xEF && data_[1] == 0xBB && data_[2] == 0xBF) {
        fail(SourceLocation{0, 1, 1}, "unexpected BOM");
        return false;
    }

    parse_value();
    if (failed_) return false;

    // 解析完顶层值后，剩余应只有空白/注释
    skip_ws_and_comments();
    if (failed_) return false;
    if (!at_end()) {
        fail(loc_, "trailing characters after JSON value");
        return false;
    }

    return true;
}

// ============================================================================
// value 分发
// ============================================================================

void JsonParser::parse_value() {
    if (failed_) return;
    skip_ws_and_comments();
    if (failed_) return;
    if (at_end()) {
        fail(loc_, "unexpected end of input");
        return;
    }
    const u8 c = peek();
    switch (c) {
        case '{': parse_object(); break;
        case '[': parse_array();  break;
        case '"': parse_string_value(); break;
        case 't': parse_literal("true", 4, true,  "true");  break;
        case 'f': parse_literal("false", 5, false, "false"); break;
        case 'n': parse_literal("null", 4, false, "null");  break;
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                parse_number();
            } else {
                fail(loc_, "unexpected character");
            }
    }
}

// ============================================================================
// literal: true/false/null
// ============================================================================

void JsonParser::parse_literal(const char* text, ca::usize len, bool bool_value,
                               const char* kind) {
    SourceLocation start = loc_;
    for (ca::usize i = 0; i < len; ++i) {
        if (pos_ + i >= byte_length_ || data_[pos_ + i] != static_cast<u8>(text[i])) {
            // 不匹配：拼一条更具体的错误
            char msg[64];
            std::snprintf(msg, sizeof(msg), "invalid literal, expected '%s'", kind);
            fail(start, msg);
            return;
        }
    }
    // 全部匹配，前进 len 字节
    for (ca::usize i = 0; i < len; ++i) advance();

    if (kind[0] == 't' || kind[0] == 'f') {
        handler_.on_bool(bool_value, start);
    } else {
        handler_.on_null(start);
    }
}

// ============================================================================
// number
// ============================================================================

void JsonParser::parse_number() {
    SourceLocation start = loc_;
    const usize begin = pos_;

    // 简单字符扫描：允许 -?[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?
    // 这里只做字符级接受，数值转换交给 strtoll/strtod（带范围与解析失败检查）。
    if (peek() == '-') advance();
    // 整数部分
    if (peek() == '0') {
        advance();
        // 0 后不能直接跟数字（不允许前导零）
        if (peek() >= '0' && peek() <= '9') {
            fail(start, "leading zeros not allowed in number");
            return;
        }
    } else if (peek() >= '1' && peek() <= '9') {
        while (peek() >= '0' && peek() <= '9') advance();
    } else {
        fail(start, "invalid number: digit expected");
        return;
    }
    bool is_float = false;
    // 小数部分
    if (peek() == '.') {
        is_float = true;
        advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            fail(start, "invalid number: digit expected after decimal point");
            return;
        }
        while (peek() >= '0' && peek() <= '9') advance();
    }
    // 指数部分
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            fail(start, "invalid number: digit expected in exponent");
            return;
        }
        while (peek() >= '0' && peek() <= '9') advance();
    }

    const usize end = pos_;
    const usize len = end - begin;
    // 复制到临时缓冲并以 \0 结尾，供 strtoll/strtod 使用
    char buf[64];
    if (len < sizeof(buf)) {
        std::memcpy(buf, data_ + begin, len);
        buf[len] = '\0';
    } else {
        // 超长数字：堆上分配
        char* heap = static_cast<char*>(std::malloc(len + 1));
        if (!heap) {
            fail(start, "out of memory parsing number");
            return;
        }
        std::memcpy(heap, data_ + begin, len);
        heap[len] = '\0';

        char* endp = nullptr;
        errno = 0;
        double d = std::strtod(heap, &endp);
        std::free(heap);
        if (errno == ERANGE) {
            fail(start, "number out of range");
            return;
        }
        if (endp != heap + len) {
            fail(start, "invalid number");
            return;
        }
        handler_.on_float(d, start);
        return;
    }

    char* endp = nullptr;
    errno = 0;
    if (is_float) {
        double d = std::strtod(buf, &endp);
        if (errno == ERANGE) { fail(start, "number out of range"); return; }
        if (endp != buf + len) { fail(start, "invalid number"); return; }
        handler_.on_float(d, start);
    } else {
        long long v = std::strtoll(buf, &endp, 10);
        if (errno == ERANGE) {
            // i64 溢出，降级为 double
            errno = 0;
            double d = std::strtod(buf, &endp);
            if (errno == ERANGE) { fail(start, "number out of range"); return; }
            if (endp != buf + len) { fail(start, "invalid number"); return; }
            handler_.on_float(d, start);
        } else if (endp != buf + len) {
            fail(start, "invalid number");
        } else {
            handler_.on_int(static_cast<i64>(v), start);
        }
    }
}

// ============================================================================
// 字符串
// ============================================================================

// 解析 4 位十六进制到 out；失败返回 false。
static bool parse_hex4(const u8* p, u32& out) {
    u32 v = 0;
    for (int i = 0; i < 4; ++i) {
        u8 c = p[i];
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

bool JsonParser::parse_string_into(ca::str::Utf8String& out) {
    assert(peek() == '"' && "parse_string_into expects current char to be '\"'");
    SourceLocation start = loc_;
    advance();  // 消费开头的 "

    ca::str::Utf8StringBuilder sb;
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
        if (c == '\\') {
            advance();  // 消费 '\'
            if (at_end()) {
                fail(start, "unterminated escape");
                return false;
            }
            const u8 esc = peek();
            advance();
            switch (esc) {
                case '"':  sb.append("\"");  break;
                case '\\': sb.append("\\");  break;
                case '/':  sb.append("/");   break;
                case 'b':  sb.append("\b");  break;
                case 'f':  sb.append("\f");  break;
                case 'n':  sb.append("\n");  break;
                case 'r':  sb.append("\r");  break;
                case 't':  sb.append("\t");  break;
                case 'u': {
                    // 需要 4 位 hex
                    if (pos_ + 4 > byte_length_) {
                        fail(start, "incomplete \\u escape");
                        return false;
                    }
                    u32 cp = 0;
                    if (!parse_hex4(data_ + pos_, cp)) {
                        fail(start, "invalid \\u escape");
                        return false;
                    }
                    for (int i = 0; i < 4; ++i) advance();
                    // 处理 surrogate pair
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        // 高代理，需要紧跟 \uXXXX 低代理
                        if (peek() == '\\' && peek_next() == 'u') {
                            advance(); advance();  // \u
                            if (pos_ + 4 > byte_length_) {
                                fail(start, "incomplete low surrogate \\u escape");
                                return false;
                            }
                            u32 low = 0;
                            if (!parse_hex4(data_ + pos_, low)) {
                                fail(start, "invalid low surrogate \\u escape");
                                return false;
                            }
                            for (int i = 0; i < 4; ++i) advance();
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                u32 full = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                if (!sb.append_code_point(full)) {
                                    fail(start, "invalid code point from surrogate pair");
                                    return false;
                                }
                            } else {
                                fail(start, "expected low surrogate after high surrogate");
                                return false;
                            }
                        } else {
                            fail(start, "dangling high surrogate");
                            return false;
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail(start, "unexpected low surrogate without high surrogate");
                        return false;
                    } else {
                        if (!sb.append_code_point(cp)) {
                            fail(start, "invalid code point");
                            return false;
                        }
                    }
                    break;
                }
                default:
                    fail(start, "invalid escape character");
                    return false;
            }
        } else if (c < 0x20) {
            // 控制字符必须转义
            fail(start, "unescaped control character in string");
            return false;
        } else {
            // 普通字节（含 UTF-8 多字节序列的任意字节）原样追加
            sb.append(&c, 1);
            advance();
        }
    }
    out = sb.build_or_empty();
    return !failed_;
}

void JsonParser::parse_string_value() {
    SourceLocation start = loc_;
    ca::str::Utf8String value;
    if (!parse_string_into(value)) return;
    handler_.on_string(std::move(value), start);
}

// ============================================================================
// array
// ============================================================================

void JsonParser::parse_array() {
    assert(peek() == '[');
    SourceLocation start = loc_;
    advance();  // 消费 '['

    ++depth_;
    if (depth_ > options_.max_depth) {
        fail(start, "nesting too deep");
        --depth_;
        return;
    }

    handler_.on_array_start(start);

    skip_ws_and_comments();
    if (failed_) { --depth_; return; }

    if (peek() == ']') {
        advance();
        handler_.on_array_end(loc_);
        --depth_;
        return;
    }

    while (!failed_) {
        parse_value();
        if (failed_) break;
        skip_ws_and_comments();
        if (failed_) break;
        const u8 c = peek();
        if (c == ',') {
            advance();
            skip_ws_and_comments();
            if (failed_) break;
            if (peek() == ']') {
                if (options_.allow_trailing_comma) {
                    advance();
                    handler_.on_array_end(loc_);
                    break;
                }
                fail(loc_, "trailing comma in array (not allowed)");
                break;
            }
        } else if (c == ']') {
            advance();
            handler_.on_array_end(loc_);
            break;
        } else {
            fail(loc_, "expected ',' or ']' in array");
            break;
        }
    }
    --depth_;
}

// ============================================================================
// object
// ============================================================================

void JsonParser::parse_object() {
    assert(peek() == '{');
    SourceLocation start = loc_;
    advance();  // 消费 '{'

    ++depth_;
    if (depth_ > options_.max_depth) {
        fail(start, "nesting too deep");
        --depth_;
        return;
    }

    handler_.on_object_start(start);

    skip_ws_and_comments();
    if (failed_) { --depth_; return; }

    if (peek() == '}') {
        advance();
        handler_.on_object_end(loc_);
        --depth_;
        return;
    }

    while (!failed_) {
        skip_ws_and_comments();
        if (failed_) break;
        // key 必须是字符串
        if (peek() != '"') {
            fail(loc_, "object key must be a string");
            break;
        }
        SourceLocation key_loc = loc_;
        ca::str::Utf8String key;
        if (!parse_string_into(key)) break;
        handler_.on_object_key(std::move(key), key_loc);

        skip_ws_and_comments();
        if (failed_) break;
        if (peek() != ':') {
            fail(loc_, "expected ':' after object key");
            break;
        }
        advance();  // 消费 ':'
        parse_value();
        if (failed_) break;

        skip_ws_and_comments();
        if (failed_) break;
        const u8 c = peek();
        if (c == ',') {
            advance();
            skip_ws_and_comments();
            if (failed_) break;
            if (peek() == '}') {
                if (options_.allow_trailing_comma) {
                    advance();
                    handler_.on_object_end(loc_);
                    break;
                }
                fail(loc_, "trailing comma in object (not allowed)");
                break;
            }
        } else if (c == '}') {
            advance();
            handler_.on_object_end(loc_);
            break;
        } else {
            fail(loc_, "expected ',' or '}' in object");
            break;
        }
    }
    --depth_;
}

}  // namespace ca::json
