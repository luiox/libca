#include "libca/yaml/yaml_writer.hpp"

#include "libca/yaml/yaml_scalar.hpp"

#include "libca/str/format.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace ca::yaml {

namespace {

bool is_control(u8 c) { return c < 0x20 || c == 0x7F; }

// ============================================================================
// 输出缓冲
// ============================================================================

class Writer {
public:
    explicit Writer(const YamlWriterOptions& options) : options_(options) {}

    ca::str::Utf8String build() { return sb_.build_or_empty(); }

    void emit(const char* s) { sb_.append(s); }
    void emit_bytes(const u8* p, ca::usize n) { sb_.append(p, n); }
    void emit_char(char c) {
        const u8 b = static_cast<u8>(c);
        sb_.append(&b, 1);
    }
    void newline() { sb_.append("\n"); }
    void indent(ca::usize depth) {
        const ca::usize total = depth * options_.indent;
        for (ca::usize i = 0; i < total; ++i) sb_.append(" ");
    }

    // ---- 标量 ----

    void write_bool(bool v) { sb_.append(v ? "true" : "false"); }

    void write_integer(ca::i64 v) {
        char buf[32];
        const int n = std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
    }

    void write_float(ca::f64 v) {
        if (std::isnan(v)) { sb_.append(".nan"); return; }
        if (std::isinf(v)) { sb_.append(v < 0 ? "-.inf" : ".inf"); return; }
        char buf[64];
        int n = std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(v));
        if (n <= 0) { sb_.append("0.0"); return; }
        bool has_dot_or_exp = false;
        for (int i = 0; i < n; ++i) {
            if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E') { has_dot_or_exp = true; break; }
        }
        if (!has_dot_or_exp && n + 2 < static_cast<int>(sizeof(buf))) {
            buf[n] = '.'; buf[n + 1] = '0'; n += 2;
        }
        sb_.append(buf, static_cast<ca::usize>(n));
    }

    // 判定：一个字符串写成 plain（不加引号）是否安全（读回仍是同一字符串）。
    static bool plain_is_safe(const u8* data, ca::usize len) {
        if (len == 0) return false;
        // plain 解析成非 String 类型 → 必须引号（如 "true"/"123"/"~"）。
        if (resolve_plain_scalar(data, len).kind != PlainScalarKind::String) return false;
        // 首尾空白。
        if (data[0] == ' ' || data[0] == '\t' || data[len - 1] == ' ' || data[len - 1] == '\t') {
            return false;
        }
        // 打头的指示符。
        switch (data[0]) {
            case '-': case '?': case ':': case ',': case '[': case ']':
            case '{': case '}': case '#': case '&': case '*': case '!':
            case '|': case '>': case '\'': case '"': case '%': case '@': case '`':
                return false;
            default: break;
        }
        for (ca::usize i = 0; i < len; ++i) {
            const u8 c = data[i];
            if (is_control(c)) return false;  // 含控制字符（\n 等）
            // ": " 或结尾 ':' → 会被当 mapping。
            if (c == ':' && (i + 1 >= len || data[i + 1] == ' ' || data[i + 1] == '\t')) return false;
            // " #" → 会被当注释。
            if (c == '#' && i > 0 && (data[i - 1] == ' ' || data[i - 1] == '\t')) return false;
        }
        return true;
    }

    // 单行字符串：plain / 单引号 / 双引号。
    void write_string_inline(const u8* data, ca::usize len) {
        if (plain_is_safe(data, len)) {
            emit_bytes(data, len);
            return;
        }
        // 含控制字符（非 \n，\n 由块标量处理，这里是被强制 inline 的回退）→ 双引号转义。
        bool has_ctrl = false;
        for (ca::usize i = 0; i < len; ++i) {
            if (is_control(data[i])) { has_ctrl = true; break; }
        }
        if (has_ctrl) {
            write_double_quoted(data, len);
        } else {
            write_single_quoted(data, len);
        }
    }

    void write_single_quoted(const u8* data, ca::usize len) {
        emit_char('\'');
        for (ca::usize i = 0; i < len; ++i) {
            if (data[i] == '\'') emit("''");
            else emit_bytes(data + i, 1);
        }
        emit_char('\'');
    }

    void write_double_quoted(const u8* data, ca::usize len) {
        emit_char('"');
        for (ca::usize i = 0; i < len; ++i) {
            const u8 c = data[i];
            switch (c) {
                case '"':  emit("\\\""); break;
                case '\\': emit("\\\\"); break;
                case '\n': emit("\\n"); break;
                case '\t': emit("\\t"); break;
                case '\r': emit("\\r"); break;
                default:
                    if (is_control(c)) {
                        char buf[8];
                        const int n = std::snprintf(buf, sizeof(buf), "\\x%02X",
                                                    static_cast<unsigned>(c));
                        if (n > 0) sb_.append(buf, static_cast<ca::usize>(n));
                    } else {
                        emit_bytes(data + i, 1);
                    }
            }
        }
        emit_char('"');
    }

    const YamlWriterOptions& options() const { return options_; }

private:
    YamlWriterOptions options_;
    ca::str::Utf8StringBuilder sb_;
};

// ============================================================================
// 递归输出
// ============================================================================

// 含换行的字符串 → 字面块标量 | 是否可保真：不含 \r / 其它控制字符，
// 且首个非空行不以空白开头（否则读回时缩进检测会误判）。
bool block_scalar_is_faithful(const u8* data, ca::usize len) {
    bool at_line_start = true;
    for (ca::usize i = 0; i < len; ++i) {
        const u8 c = data[i];
        if (c == '\n') { at_line_start = true; continue; }
        if (c == '\r') return false;
        if (is_control(c)) return false;
        if (at_line_start && (c == ' ' || c == '\t')) return false;
        at_line_start = false;
    }
    return true;
}

// 输出多行字符串为字面块标量正文（每行加 depth 缩进）。header 已由调用方写好。
void write_block_scalar_body(Writer& w, const u8* data, ca::usize len, ca::usize depth) {
    ca::usize i = 0;
    // 逐行（含末尾）。chomping header 已按尾换行数选好，这里正文不再补/删尾换行。
    // 去掉最后一个 '\n'（它对应 header 的 clip/keep 语义，由行结构表达）。
    ca::usize effective = len;
    while (effective > 0 && data[effective - 1] == '\n') --effective;
    ca::usize trailing_nl = len - effective;

    ca::usize line_start = 0;
    for (i = 0; i <= effective; ++i) {
        if (i == effective || data[i] == '\n') {
            w.indent(depth);
            if (i > line_start) w.emit_bytes(data + line_start, i - line_start);
            w.newline();
            line_start = i + 1;
        }
    }
    // keep（|+）的额外尾空行。
    for (ca::usize k = 1; k < trailing_nl; ++k) {
        w.newline();
    }
}

// 选择块标量 header 并返回 chomping 后缀。尾换行：0→"-"(strip)，1→""(clip)，≥2→"+"(keep)。
const char* block_chomp_suffix(const u8* data, ca::usize len) {
    ca::usize trailing = 0;
    ca::usize i = len;
    while (i > 0 && data[i - 1] == '\n') { ++trailing; --i; }
    if (trailing == 0) return "-";
    if (trailing == 1) return "";
    return "+";
}

void write_node(Writer& w, const YamlValue& v, ca::usize depth);

void write_key(Writer& w, const ca::str::Utf8StringRef& key) {
    w.write_string_inline(key.data(), key.byte_length());
}

// 输出映射（不含前导缩进；调用方负责首行定位）。first_inline=true 表示首个 key
// 承接调用方已写的前缀（如 "- "），不再自缩进。
void write_mapping(Writer& w, const YamlValue& v, ca::usize depth, bool first_inline) {
    const auto& members = v.as_mapping();
    bool first = true;
    for (const auto& m : members) {
        if (!(first && first_inline)) w.indent(depth);
        first = false;
        write_key(w, m.first);
        w.emit_char(':');
        const YamlValue& child = m.second;
        if (child.is_mapping()) {
            if (child.as_mapping().empty()) {
                w.emit(" {}");
                w.newline();
            } else {
                w.newline();
                write_mapping(w, child, depth + 1, false);
            }
        } else if (child.is_sequence()) {
            if (child.as_sequence().empty()) {
                w.emit(" []");
                w.newline();
            } else {
                w.newline();
                write_node(w, child, depth + 1);
            }
        } else if (child.is_string() &&
                   [&] {
                       const auto& s = child.as_string();
                       const u8* d = s.data();
                       const ca::usize n = s.byte_length();
                       for (ca::usize i = 0; i < n; ++i) if (d[i] == '\n') return true;
                       return false;
                   }()) {
            // 多行字符串
            const auto& s = child.as_string();
            if (block_scalar_is_faithful(s.data(), s.byte_length())) {
                w.emit(" |");
                w.emit(block_chomp_suffix(s.data(), s.byte_length()));
                w.newline();
                write_block_scalar_body(w, s.data(), s.byte_length(), depth + 1);
            } else {
                w.emit_char(' ');
                w.write_double_quoted(s.data(), s.byte_length());
                w.newline();
            }
        } else {
            w.emit_char(' ');
            write_node(w, child, depth);  // 标量单行，depth 不影响
            w.newline();
        }
    }
}

void write_sequence(Writer& w, const YamlValue& v, ca::usize depth) {
    const auto& items = v.as_sequence();
    for (const auto& item : items) {
        w.indent(depth);
        w.emit_char('-');
        if (item.is_mapping() && !item.as_mapping().empty()) {
            w.emit_char(' ');
            write_mapping(w, item, depth + 1, /*first_inline=*/true);
        } else if (item.is_sequence() && !item.as_sequence().empty()) {
            w.newline();
            write_sequence(w, item, depth + 1);
        } else if (item.is_mapping()) {
            w.emit(" {}");
            w.newline();
        } else if (item.is_sequence()) {
            w.emit(" []");
            w.newline();
        } else if (item.is_string() &&
                   [&] {
                       const auto& s = item.as_string();
                       const u8* d = s.data();
                       const ca::usize n = s.byte_length();
                       for (ca::usize i = 0; i < n; ++i) if (d[i] == '\n') return true;
                       return false;
                   }()) {
            const auto& s = item.as_string();
            if (block_scalar_is_faithful(s.data(), s.byte_length())) {
                w.emit(" |");
                w.emit(block_chomp_suffix(s.data(), s.byte_length()));
                w.newline();
                write_block_scalar_body(w, s.data(), s.byte_length(), depth + 1);
            } else {
                w.emit_char(' ');
                w.write_double_quoted(s.data(), s.byte_length());
                w.newline();
            }
        } else {
            w.emit_char(' ');
            write_node(w, item, depth);
            w.newline();
        }
    }
}

// 输出一个标量或（非空）集合。集合由 mapping/sequence 分派；标量单行不换行。
void write_node(Writer& w, const YamlValue& v, ca::usize depth) {
    switch (v.type()) {
        case YamlType::Null:    w.emit("null"); break;
        case YamlType::Boolean: w.write_bool(v.as_boolean()); break;
        case YamlType::Integer: w.write_integer(v.as_integer()); break;
        case YamlType::Float:   w.write_float(v.as_float()); break;
        case YamlType::String: {
            const auto& s = v.as_string();
            w.write_string_inline(s.data(), s.byte_length());
            break;
        }
        case YamlType::Sequence: write_sequence(w, v, depth); break;
        case YamlType::Mapping:  write_mapping(w, v, depth, false); break;
    }
}

}  // namespace

ca::str::Utf8String YamlWriter::write(const YamlDocument& document,
                                      const YamlWriterOptions& options) {
    Writer w(options);
    const YamlValue& root = document.root();
    if (root.is_mapping()) {
        if (root.as_mapping().empty()) {
            w.emit("{}");
            w.newline();
        } else {
            write_mapping(w, root, 0, false);
        }
    } else if (root.is_sequence()) {
        if (root.as_sequence().empty()) {
            w.emit("[]");
            w.newline();
        } else {
            write_sequence(w, root, 0);
        }
    } else {
        // 根标量：单行 + 换行。多行字符串亦按块标量输出。
        if (root.is_string()) {
            const auto& s = root.as_string();
            const u8* d = s.data();
            const ca::usize n = s.byte_length();
            bool multiline = false;
            for (ca::usize i = 0; i < n; ++i) if (d[i] == '\n') { multiline = true; break; }
            if (multiline) {
                if (block_scalar_is_faithful(d, n)) {
                    // 根块标量无 key 前缀，直接写 header。
                    Writer& ww = w;
                    ww.emit("|");
                    ww.emit(block_chomp_suffix(d, n));
                    ww.newline();
                    write_block_scalar_body(ww, d, n, 1);
                    return w.build();
                }
                w.write_double_quoted(d, n);
                w.newline();
                return w.build();
            }
        }
        write_node(w, root, 0);
        w.newline();
    }
    return w.build();
}

ca::Result<void, ca::str::Utf8String> YamlWriter::write_file(const ca::str::Utf8StringRef& path,
                                                             const YamlDocument& document,
                                                             const YamlWriterOptions& options) {
    ca::str::Utf8String text = write(document, options);
    std::string path_str(reinterpret_cast<const char*>(path.data()),
                         reinterpret_cast<const char*>(path.data()) + path.byte_length());
    std::ofstream out(std::filesystem::u8path(path_str), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ca::str::format_std("failed to open YAML file for writing: {}", path_str).c_str()));
    }
    out.write(reinterpret_cast<const char*>(text.data()), static_cast<std::streamsize>(text.byte_length()));
    if (!out.good()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ca::str::format_std("failed to write YAML file: {}", path_str).c_str()));
    }
    return ca::Ok();
}

}  // namespace ca::yaml
