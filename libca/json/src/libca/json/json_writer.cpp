#include "libca/json/json_writer.hpp"

#include "libca/str/utf8_string.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

namespace ca::json {

namespace {

// 输出缓冲：封装 Utf8StringBuilder + pretty 缩进状态。
class Writer {
public:
    Writer(const JsonWriterOptions& options) : options_(options) {}

    ca::str::Utf8String build() { return sb_.build_or_empty(); }

    void write_null()    { sb_.append("null"); }
    void write_bool(bool v) { sb_.append(v ? "true" : "false"); }
    void write_int(ca::i64 v) {
        char buf[32];
        int n = std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
    }
    void write_float(ca::f64 v) {
        // RFC 8259 不允许 NaN/Infinity（未定义字面量）；snprintf("%g") 会输出 "nan"/"inf"
        // 产生非法 JSON。这里统一序列化为 null。
        if (std::isnan(v) || std::isinf(v)) {
            sb_.append("null");
            return;
        }
        char buf[64];
        // %.17g 保证 double round-trip 精度
        int n = std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(v));
        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
    }

    void write_raw(const char* s, ca::usize len) { sb_.append(s, len); }

    // 字符串转义输出。
    void write_string(const ca::str::Utf8StringRef& s) {
        sb_.append("\"");
        const u8* data = s.data();
        const usize len = s.byte_length();
        for (usize i = 0; i < len; ++i) {
            const u8 c = data[i];
            switch (c) {
                case '"':  sb_.append("\\\""); break;
                case '\\': sb_.append("\\\\"); break;
                case '\b': sb_.append("\\b");  break;
                case '\f': sb_.append("\\f");  break;
                case '\n': sb_.append("\\n");  break;
                case '\r': sb_.append("\\r");  break;
                case '\t': sb_.append("\\t");  break;
                default:
                    if (c < 0x20) {
                        // 控制字符转 \u00XX
                        char buf[8];
                        int n = std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
                    } else if (options_.ensure_ascii && c >= 0x80) {
                        // 非 ASCII：解码 UTF-8 码点，输出 \uXXXX（BMP）或 surrogate pair
                        write_escaped_utf8(data, len, i);
                    } else {
                        sb_.append(&c, 1);
                    }
            }
        }
        sb_.append("\"");
    }

    // ---- pretty 缩进 ----

    bool pretty() const { return options_.pretty; }

    void newline_and_indent() {
        if (!options_.pretty) return;
        sb_.append("\n");
        const usize total = depth_ * options_.indent;
        for (usize i = 0; i < total; ++i) sb_.append(" ");
    }

    void push_depth() { ++depth_; }
    void pop_depth()  { --depth_; }

    void space() { if (options_.pretty) sb_.append(" "); }

private:
    ca::str::Utf8StringBuilder sb_;
    JsonWriterOptions options_;
    usize depth_ = 0;

    // 从 data[i] 开始解码一个 UTF-8 码点，输出转义形式，并推进 i。
    void write_escaped_utf8(const u8* data, usize len, usize& i) {
        // 解码 UTF-8
        u32 cp = 0;
        usize extra = 0;
        const u8 c = data[i];
        if ((c & 0x80) == 0) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F; extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F; extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07; extra = 3;
        } else {
            // 非法首字节：当作单字节
            char buf[8];
            int n = std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
            return;
        }
        if (i + extra >= len) {
            // 截断，同上兜底
            char buf[8];
            int n = std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
            return;
        }
        for (usize k = 1; k <= extra; ++k) {
            const u8 cc = data[i + k];
            if ((cc & 0xC0) != 0x80) {
                // 非法续字节
                char buf[8];
                int n = std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
                return;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        i += extra;
        // 输出
        if (cp <= 0xFFFF) {
            char buf[8];
            int n = std::snprintf(buf, sizeof(buf), "\\u%04x", cp);
            if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
        } else {
            // 输出 surrogate pair
            const u32 v = cp - 0x10000;
            const u32 hi = 0xD800 + (v >> 10);
            const u32 lo = 0xDC00 + (v & 0x3FF);
            char buf[16];
            int n = std::snprintf(buf, sizeof(buf), "\\u%04x\\u%04x", hi, lo);
            if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
        }
    }
};

void write_value(Writer& w, const JsonValue& v);

void write_array(Writer& w, const JsonValue& v) {
    const auto& arr = v.as_array();
    if (arr.empty()) { w.write_raw("[]", 2); return; }
    w.write_raw("[", 1);
    w.push_depth();
    for (ca::usize i = 0; i < arr.size(); ++i) {
        if (i > 0) w.write_raw(",", 1);
        w.newline_and_indent();
        write_value(w, arr[i]);
    }
    w.pop_depth();
    w.newline_and_indent();
    w.write_raw("]", 1);
}

void write_object(Writer& w, const JsonValue& v) {
    const auto& obj = v.as_object();
    if (obj.empty()) { w.write_raw("{}", 2); return; }
    w.write_raw("{", 1);
    w.push_depth();
    for (ca::usize i = 0; i < obj.size(); ++i) {
        if (i > 0) w.write_raw(",", 1);
        w.newline_and_indent();
        w.write_string(obj[i].first);
        w.write_raw(":", 1);
        w.space();
        write_value(w, obj[i].second);
    }
    w.pop_depth();
    w.newline_and_indent();
    w.write_raw("}", 1);
}

void write_value(Writer& w, const JsonValue& v) {
    switch (v.type()) {
        case JsonType::Null:   w.write_null(); break;
        case JsonType::Bool:   w.write_bool(v.as_bool()); break;
        case JsonType::Int:    w.write_int(v.as_int()); break;
        case JsonType::Float:  w.write_float(v.as_float()); break;
        case JsonType::String: w.write_string(v.as_string()); break;
        case JsonType::Array:
            if (w.pretty()) write_array(w, v);
            else {
                const auto& arr = v.as_array();
                w.write_raw("[", 1);
                for (ca::usize i = 0; i < arr.size(); ++i) {
                    if (i > 0) w.write_raw(",", 1);
                    write_value(w, arr[i]);
                }
                w.write_raw("]", 1);
            }
            break;
        case JsonType::Object:
            if (w.pretty()) write_object(w, v);
            else {
                const auto& obj = v.as_object();
                w.write_raw("{", 1);
                for (ca::usize i = 0; i < obj.size(); ++i) {
                    if (i > 0) w.write_raw(",", 1);
                    w.write_string(obj[i].first);
                    w.write_raw(":", 1);
                    write_value(w, obj[i].second);
                }
                w.write_raw("}", 1);
            }
            break;
    }
}

}  // namespace

ca::str::Utf8String JsonWriter::write(const JsonDocument& document,
                                       const JsonWriterOptions& options) {
    Writer w(options);
    write_value(w, document.root());
    return w.build();
}

ca::Result<void, ca::str::Utf8String> JsonWriter::write_file(
    const ca::str::Utf8StringRef& path,
    const JsonDocument& document,
    const JsonWriterOptions& options) {
    ca::str::Utf8String text = write(document, options);
    std::string path_str(path.data(), path.data() + path.byte_length());
    std::ofstream out(path_str, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return ca::Err(ca::str::Utf8String::from_cstr("failed to open file for writing"));
    }
    out.write(reinterpret_cast<const char*>(text.data()),
              static_cast<std::streamsize>(text.byte_length()));
    if (!out.good()) {
        return ca::Err(ca::str::Utf8String::from_cstr("failed to write JSON file"));
    }
    return ca::Ok();
}

}  // namespace ca::json
