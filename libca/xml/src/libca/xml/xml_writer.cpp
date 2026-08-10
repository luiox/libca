#include "libca/xml/xml_writer.hpp"

#include "libca/str/format.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace ca::xml {

namespace {

class Writer {
public:
    explicit Writer(const XmlWriterOptions& options) : options_(options) {}

    ca::str::Utf8String build() { return sb_.build_or_empty(); }

    void emit(const char* s) { sb_.append(s); }
    void emit_bytes(const u8* p, ca::usize n) { sb_.append(p, n); }
    void emit_char(char c) {
        const u8 b = static_cast<u8>(c);
        sb_.append(&b, 1);
    }
    void newline() { emit_char('\n'); }
    void indent(ca::usize depth) {
        const ca::usize total = depth * options_.indent;
        for (ca::usize i = 0; i < total; ++i) emit_char(' ');
    }

    // 文本内容转义：& < >（> 转义以规避 ]]> 序列，稳妥）。
    void emit_text_escaped(const u8* data, ca::usize len) {
        for (ca::usize i = 0; i < len; ++i) {
            switch (data[i]) {
                case '&': emit("&amp;"); break;
                case '<': emit("&lt;"); break;
                case '>': emit("&gt;"); break;
                default:  emit_bytes(data + i, 1);
            }
        }
    }

    // 属性值转义：& < " 以及制表/换行/回车（用数字引用保真，双引号包裹）。
    void emit_attr_escaped(const u8* data, ca::usize len) {
        for (ca::usize i = 0; i < len; ++i) {
            switch (data[i]) {
                case '&':  emit("&amp;"); break;
                case '<':  emit("&lt;"); break;
                case '"':  emit("&quot;"); break;
                case '\t': emit("&#x9;"); break;
                case '\n': emit("&#xA;"); break;
                case '\r': emit("&#xD;"); break;
                default:   emit_bytes(data + i, 1);
            }
        }
    }

private:
    XmlWriterOptions options_;
    ca::str::Utf8StringBuilder sb_;
};

bool element_has_text(const XmlNode& el) {
    for (const auto& c : el.children()) {
        if (c.is_text() || c.is_cdata()) return true;
    }
    return false;
}

void emit_open_tag(Writer& w, const XmlNode& el, bool& self_closed) {
    w.emit_char('<');
    const auto& n = el.name();
    w.emit_bytes(n.data(), n.byte_length());
    for (const auto& attr : el.attributes()) {
        w.emit_char(' ');
        w.emit_bytes(attr.first.data(), attr.first.byte_length());
        w.emit("=\"");
        w.emit_attr_escaped(attr.second.data(), attr.second.byte_length());
        w.emit_char('"');
    }
    if (el.children().empty()) {
        w.emit("/>");
        self_closed = true;
    } else {
        self_closed = false;
    }
}

void emit_leaf_inline(Writer& w, const XmlNode& node) {
    switch (node.type()) {
        case XmlNodeType::Text:
            w.emit_text_escaped(node.value().data(), node.value().byte_length());
            break;
        case XmlNodeType::Cdata:
            w.emit("<![CDATA[");
            w.emit_bytes(node.value().data(), node.value().byte_length());
            w.emit("]]>");
            break;
        case XmlNodeType::Comment:
            w.emit("<!--");
            w.emit_bytes(node.value().data(), node.value().byte_length());
            w.emit("-->");
            break;
        case XmlNodeType::Element:
            break;  // 由 write_inline 处理
    }
}

// 行内输出一个节点（混合内容场景：不加任何缩进/换行）。
void write_inline(Writer& w, const XmlNode& node) {
    if (!node.is_element()) {
        emit_leaf_inline(w, node);
        return;
    }
    bool self_closed = false;
    emit_open_tag(w, node, self_closed);
    if (self_closed) return;
    w.emit_char('>');
    for (const auto& c : node.children()) write_inline(w, c);
    w.emit("</");
    w.emit_bytes(node.name().data(), node.name().byte_length());
    w.emit_char('>');
}

void write_element(Writer& w, const XmlNode& el, ca::usize depth);

// 块模式下输出一个子节点（元素或注释；文本/CDATA 不会走到这里）。
void write_block_child(Writer& w, const XmlNode& node, ca::usize depth) {
    if (node.is_element()) {
        write_element(w, node, depth);
    } else {
        // 注释（块模式）
        w.indent(depth);
        emit_leaf_inline(w, node);
        w.newline();
    }
}

// 缩进美化输出一个元素。调用方负责本元素的前导缩进由本函数打（indent(depth)）。
void write_element(Writer& w, const XmlNode& el, ca::usize depth) {
    w.indent(depth);
    bool self_closed = false;
    emit_open_tag(w, el, self_closed);
    if (self_closed) {
        w.newline();
        return;
    }
    if (element_has_text(el)) {
        // 混合内容：行内保真。
        w.emit_char('>');
        for (const auto& c : el.children()) write_inline(w, c);
        w.emit("</");
        w.emit_bytes(el.name().data(), el.name().byte_length());
        w.emit_char('>');
        w.newline();
        return;
    }
    // 纯元素/注释子节点：分行缩进。
    w.emit_char('>');
    w.newline();
    for (const auto& c : el.children()) write_block_child(w, c, depth + 1);
    w.indent(depth);
    w.emit("</");
    w.emit_bytes(el.name().data(), el.name().byte_length());
    w.emit_char('>');
    w.newline();
}

void write_declaration(Writer& w, const XmlDeclaration& decl) {
    if (!decl.present) return;
    w.emit("<?xml version=\"");
    if (decl.version.byte_length() > 0) {
        w.emit_bytes(decl.version.data(), decl.version.byte_length());
    } else {
        w.emit("1.0");
    }
    w.emit_char('"');
    if (decl.encoding.byte_length() > 0) {
        w.emit(" encoding=\"");
        w.emit_bytes(decl.encoding.data(), decl.encoding.byte_length());
        w.emit_char('"');
    }
    if (decl.standalone.byte_length() > 0) {
        w.emit(" standalone=\"");
        w.emit_bytes(decl.standalone.data(), decl.standalone.byte_length());
        w.emit_char('"');
    }
    w.emit("?>");
    w.newline();
}

}  // namespace

ca::str::Utf8String XmlWriter::write(const XmlDocument& document, const XmlWriterOptions& options) {
    Writer w(options);
    write_declaration(w, document.declaration());
    for (const auto& node : document.prolog()) write_block_child(w, node, 0);

    const XmlNode& root = document.root();
    if (root.is_element()) {
        write_element(w, root, 0);
    } else if (!root.is_text() || root.value().byte_length() > 0) {
        // 非常规根（非元素）：尽力单行输出，末尾换行。
        write_inline(w, root);
        w.newline();
    }

    for (const auto& node : document.epilog()) write_block_child(w, node, 0);
    return w.build();
}

ca::Result<void, ca::str::Utf8String> XmlWriter::write_file(const ca::str::Utf8StringRef& path,
                                                            const XmlDocument& document,
                                                            const XmlWriterOptions& options) {
    ca::str::Utf8String text = write(document, options);
    std::string path_str(reinterpret_cast<const char*>(path.data()),
                         reinterpret_cast<const char*>(path.data()) + path.byte_length());
    std::ofstream out(path_str, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ca::str::format_std("failed to open XML file for writing: {}", path_str).c_str()));
    }
    out.write(reinterpret_cast<const char*>(text.data()),
              static_cast<std::streamsize>(text.byte_length()));
    if (!out.good()) {
        return ca::Err(ca::str::Utf8String::from_cstr(
            ca::str::format_std("failed to write XML file: {}", path_str).c_str()));
    }
    return ca::Ok();
}

}  // namespace ca::xml
