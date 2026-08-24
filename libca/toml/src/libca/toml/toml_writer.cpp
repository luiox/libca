#include "libca/toml/toml_writer.hpp"

#include "libca/str/utf8_string.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace ca::toml {

namespace {

// ============================================================================
// 输出缓冲
// ============================================================================

class Writer {
public:
    Writer(const TomlWriterOptions& options) : options_(options) {}

    ca::str::Utf8String build() { return sb_.build_or_empty(); }

    void emit(const char* s) { sb_.append(s); }
    void emit(const char* s, ca::usize len) {
        sb_.append(s, len);
    }
    void emit_char(char c) {
        u8 b = static_cast<u8>(c);
        sb_.append(&b, 1);
    }

    void newline() { sb_.append("\n"); }

    void indent_to(ca::usize depth) {
        if (!options_.indent_subtables) return;
        const ca::usize total = depth * options_.indent;
        for (ca::usize i = 0; i < total; ++i) sb_.append(" ");
    }

    // ---- 标量序列化 ----

    void write_bool(bool v) { sb_.append(v ? "true" : "false"); }

    void write_integer(ca::i64 v) {
        char buf[32];
        int n = std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
    }

    void write_float(ca::f64 v) {
        if (std::isnan(v)) { sb_.append("nan"); return; }
        if (std::isinf(v)) { sb_.append(v < 0 ? "-inf" : "inf"); return; }
        // %.17g 保证 round-trip；但需保证结果至少含一个 '.' 或 'e'，
        // 否则 TOML 解析器会把整数形态当作 Integer。
        char buf[64];
        int n = std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(v));
        if (n <= 0) { sb_.append("0.0"); return; }
        // 检查是否含 '.' 或 'e'/'E'
        bool has_dot_or_exp = false;
        for (int i = 0; i < n; ++i) {
            if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E') { has_dot_or_exp = true; break; }
        }
        if (!has_dot_or_exp) {
            // 整数形态（如 "1"），追加 ".0"
            if (n + 2 < (int)sizeof(buf)) {
                buf[n] = '.'; buf[n+1] = '0'; n += 2;
            }
        }
        sb_.append(buf, static_cast<ca::usize>(n));
    }

    void write_datetime(const TomlDatetime& dt) {
        char buf[64];
        int n = 0;
        switch (dt.kind) {
            case TomlDatetimeKind::LocalDate:
                n = std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u",
                                  static_cast<int>(dt.year),
                                  static_cast<unsigned>(dt.month),
                                  static_cast<unsigned>(dt.day));
                break;
            case TomlDatetimeKind::LocalTime:
                if (dt.nanos == 0) {
                    n = std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                                      static_cast<unsigned>(dt.hour),
                                      static_cast<unsigned>(dt.minute),
                                      static_cast<unsigned>(dt.second));
                } else {
                    double sec = dt.second + dt.nanos / 1e9;
                    n = std::snprintf(buf, sizeof(buf), "%02u:%02u:%09.9f",
                                      static_cast<unsigned>(dt.hour),
                                      static_cast<unsigned>(dt.minute),
                                      sec);
                    // 截断到合理位数
                }
                break;
            case TomlDatetimeKind::LocalDateTime:
            case TomlDatetimeKind::OffsetDatetime: {
                // 日期 + 时间
                if (dt.nanos == 0) {
                    n = std::snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02u:%02u:%02u",
                                      static_cast<int>(dt.year),
                                      static_cast<unsigned>(dt.month),
                                      static_cast<unsigned>(dt.day),
                                      static_cast<unsigned>(dt.hour),
                                      static_cast<unsigned>(dt.minute),
                                      static_cast<unsigned>(dt.second));
                } else {
                    // 带小数秒：把纳秒规范化为去掉末尾 0 的十进制小数。
                    // 直接拼装：sec.sssssssss 形式，但要去掉末尾 0。
                    // 为简单，用 %.9f 后裁剪尾 0。
                    double sec = dt.second + dt.nanos / 1e9;
                    char tmp[64];
                    int m = std::snprintf(tmp, sizeof(tmp), "%02u:%02u:%.9f",
                                          static_cast<unsigned>(dt.hour),
                                          static_cast<unsigned>(dt.minute),
                                          sec);
                    // 裁剪 tmp 中 . 之后尾随的 0
                    if (m > 0) {
                        // 找到 '.' 位置
                        int dot = -1;
                        for (int i = 0; i < m; ++i) {
                            if (tmp[i] == '.') { dot = i; break; }
                        }
                        if (dot >= 0) {
                            int end = m;
                            while (end - 1 > dot && tmp[end-1] == '0') --end;
                            if (end - 1 == dot) end = dot;  // 整数秒：去掉小数点
                            m = end;
                        }
                    }
                    n = std::snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%.*s",
                                      static_cast<int>(dt.year),
                                      static_cast<unsigned>(dt.month),
                                      static_cast<unsigned>(dt.day),
                                      m, tmp);
                }
                if (dt.kind == TomlDatetimeKind::OffsetDatetime && dt.has_tz) {
                    // 追加时区
                    if (dt.tz_minutes == 0) {
                        n += std::snprintf(buf + n, sizeof(buf) - n, "Z");
                    } else {
                        ca::i16 off = dt.tz_minutes;
                        char sign = off < 0 ? '-' : '+';
                        ca::i16 abs_min = static_cast<ca::i16>(off < 0 ? -off : off);
                        int hh = abs_min / 60;
                        int mm = abs_min % 60;
                        n += std::snprintf(buf + n, sizeof(buf) - n, "%c%02d:%02d",
                                           sign, hh, mm);
                    }
                }
                break;
            }
        }
        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
    }

    // ---- 字符串序列化 ----
    // 规则：
    //   - 含换行符 → multiline basic string（"""..."""），转义必要的 \ 与 """
    //   - 否则 → basic string（单行），转义 " \ 控制 \u
    void write_string(const ca::str::Utf8StringRef& s) {
        const u8* data = s.data();
        const usize len = s.byte_length();
        bool has_newline = false;
        for (usize i = 0; i < len; ++i) {
            if (data[i] == '\n') { has_newline = true; break; }
        }
        if (has_newline) {
            write_multiline_basic_string(data, len);
        } else {
            write_basic_string(data, len);
        }
    }

    void write_basic_string(const u8* data, usize len) {
        sb_.append("\"");
        for (usize i = 0; i < len; ++i) {
            const u8 c = data[i];
            switch (c) {
                case '"':  sb_.append("\\\""); break;
                case '\\': sb_.append("\\\\"); break;
                case '\b': sb_.append("\\b"); break;
                case '\t': sb_.append("\\t"); break;
                case '\n': sb_.append("\\n"); break;
                case '\f': sb_.append("\\f"); break;
                case '\r': sb_.append("\\r"); break;
                default:
                    if (c < 0x20 || c == 0x7F) {  // DEL 不在 TOML 1.0 未转义字符集，原样写出会被严格解析器拒绝
                        char buf[8];
                        int n = std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
                    } else {
                        sb_.append(&c, 1);
                    }
            }
        }
        sb_.append("\"");
    }

    void write_multiline_basic_string(const u8* data, usize len) {
        sb_.append("\"\"\"\n");
        for (usize i = 0; i < len; ++i) {
            const u8 c = data[i];
            // 防止意外闭合 """：若接下来是 """，转义中间引号
            if (c == '"' && i + 2 < len && data[i+1] == '"' && data[i+2] == '"') {
                sb_.append("\\\"\\\"\\\"");
                i += 2;
                continue;
            }
            switch (c) {
                case '\\': sb_.append("\\\\"); break;
                case '\r': sb_.append("\\r"); break;
                case '\t': sb_.append("\t"); break;
                case '\n': sb_.append("\n"); break;
                default:
                    if (c < 0x20 || c == 0x7F) {  // DEL 不在 TOML 1.0 未转义字符集，原样写出会被严格解析器拒绝
                        char buf[8];
                        int n = std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
                    } else {
                        sb_.append(&c, 1);
                    }
            }
        }
        sb_.append("\"\"\"");
    }

    // ---- key 序列化 ----
    // bare key（A-Za-z0-9_- 且非空）原样输出；否则用 basic string 包裹。
    void write_key(const ca::str::Utf8StringRef& key) {
        const u8* data = key.data();
        const usize len = key.byte_length();
        if (len == 0) { sb_.append("\"\""); return; }
        bool bare = true;
        for (usize i = 0; i < len; ++i) {
            const u8 c = data[i];
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '_' || c == '-';
            if (!ok) { bare = false; break; }
        }
        if (bare) {
            sb_.append(data, len);
        } else {
            write_basic_string(data, len);
        }
    }

    const TomlWriterOptions& options() const { return options_; }

private:
    ca::str::Utf8StringBuilder sb_;
    TomlWriterOptions options_;
};

// ============================================================================
// Value 序列化（用于 = 右侧的值，或数组元素）
// ============================================================================

void write_inline_value(Writer& w, const TomlValue& v);

void write_inline_array(Writer& w, const TomlValue& v) {
    const auto& arr = v.as_array();
    w.emit("[");
    for (ca::usize i = 0; i < arr.size(); ++i) {
        if (i > 0) w.emit(", ");
        write_inline_value(w, arr[i]);
    }
    w.emit("]");
}

void write_inline_table(Writer& w, const TomlValue& v) {
    const auto& tbl = v.as_table();
    w.emit("{");
    for (ca::usize i = 0; i < tbl.size(); ++i) {
        if (i > 0) w.emit(", ");
        w.write_key(tbl[i].first);
        w.emit(" = ");
        write_inline_value(w, tbl[i].second);
    }
    w.emit("}");
}

void write_inline_value(Writer& w, const TomlValue& v) {
    switch (v.type()) {
        case TomlType::String:          w.write_string(v.as_string()); break;
        case TomlType::Integer:         w.write_integer(v.as_integer()); break;
        case TomlType::Float:           w.write_float(v.as_float()); break;
        case TomlType::Boolean:         w.write_bool(v.as_boolean()); break;
        case TomlType::OffsetDatetime:  w.write_datetime(v.as_offset_datetime()); break;
        case TomlType::LocalDateTime:   w.write_datetime(v.as_local_datetime()); break;
        case TomlType::LocalDate:       w.write_datetime(v.as_local_date()); break;
        case TomlType::LocalTime:       w.write_datetime(v.as_local_time()); break;
        case TomlType::Array:           write_inline_array(w, v); break;
        case TomlType::Table:           write_inline_table(w, v); break;
    }
}

// 判断一个 value 是否应当作为子表（用 [header] 形式输出）而非 inline。
// 规则：是 Table 且非空 → 子表；是 Array 且元素全为 Table → 数组表。
bool is_table_like(const TomlValue& v) {
    if (v.is_table()) return !v.as_table().empty();
    if (v.is_array()) {
        const auto& arr = v.as_array();
        if (arr.empty()) return false;
        for (const auto& e : arr) {
            if (!e.is_table()) return false;
        }
        return true;
    }
    return false;
}

// ============================================================================
// 文档级序列化
// ============================================================================

// path：从 root 到当前 table 的路径段（用于构造 [a.b.c] 表头）。
void write_table(Writer& w, const TomlValue& tbl,
                 const std::vector<ca::str::Utf8StringRef>& path) {
    // 1) 先输出本表的"非子表"成员（标量/数组/inline table）为 key = value。
    bool wrote_kv = false;
    for (const auto& m : tbl.as_table()) {
        if (is_table_like(m.second)) continue;
        w.indent_to(path.size());
        w.write_key(m.first);
        w.emit(" = ");
        write_inline_value(w, m.second);
        w.newline();
        wrote_kv = true;
    }
    // 2) 输出本表的子表：递归。
    //    对子表（非 array-of-tables）：[a.b]
    //    对数组表：[[a.b]]，每个元素一个段。
    for (const auto& m : tbl.as_table()) {
        if (!is_table_like(m.second)) continue;
        std::vector<ca::str::Utf8StringRef> sub_path = path;
        sub_path.push_back(m.first);
        if (m.second.is_table()) {
            // 标准 sub-table
            if (wrote_kv || path.empty()) {
                // 已经有 KV 或子表分段时插入空行分隔；root 直接子表也空行分隔。
                // （但仅当不是本表的第一个输出时）
            }
            w.newline();
            w.indent_to(path.size());
            w.emit("[");
            for (ca::usize i = 0; i < sub_path.size(); ++i) {
                if (i > 0) w.emit(".");
                w.write_key(sub_path[i]);
            }
            w.emit("]");
            w.newline();
            write_table(w, m.second, sub_path);
        } else {
            // array of tables
            for (const auto& elem : m.second.as_array()) {
                w.newline();
                w.indent_to(path.size());
                w.emit("[[");
                for (ca::usize i = 0; i < sub_path.size(); ++i) {
                    if (i > 0) w.emit(".");
                    w.write_key(sub_path[i]);
                }
                w.emit("]]");
                w.newline();
                write_table(w, elem, sub_path);
            }
        }
    }
}

}  // namespace

ca::str::Utf8String TomlWriter::write(const TomlDocument& document,
                                      const TomlWriterOptions& options) {
    Writer w(options);
    // root 本身不输出表头，直接从 root 开始递归。
    std::vector<ca::str::Utf8StringRef> path;
    if (document.root().is_table()) {
        write_table(w, document.root(), path);
    } else {
        // 非常规：root 不是 table，原样输出（理论不应出现）。
        write_inline_value(w, document.root());
        w.newline();
    }
    return w.build();
}

ca::Result<void, ca::str::Utf8String> TomlWriter::write_file(
    const ca::str::Utf8StringRef& path,
    const TomlDocument& document,
    const TomlWriterOptions& options) {
    ca::str::Utf8String text = write(document, options);
    std::string path_str(path.data(), path.data() + path.byte_length());
    std::ofstream out(path_str, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return ca::Err(ca::str::Utf8String::from_cstr("failed to open file for writing"));
    }
    out.write(reinterpret_cast<const char*>(text.data()),
              static_cast<std::streamsize>(text.byte_length()));
    if (!out.good()) {
        return ca::Err(ca::str::Utf8String::from_cstr("failed to write TOML file"));
    }
    return ca::Ok();
}

}  // namespace ca::toml
