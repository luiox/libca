#pragma once

/// @file xml_node.hpp
/// @brief XML DOM 数据模型：XmlNode。
/// @details XmlNode 是统一的 DOM 树节点，用 `XmlNodeType` 枚举区分 4 种形态：
///          Element（元素：名字 + 属性 + 子节点）/ Text（文本）/ Comment（注释）/
///          Cdata（CDATA 段）。与 toml/yaml 同构：所有字符串（元素名、属性名/值、文本内容）
///          都是 `Utf8StringRef`，指向所属 `XmlDocument` 内部的 `Utf8StringArena`，
///          节点因此**可拷贝**（浅拷贝引用）。
/// @note 支持**混合内容**：一个元素的子节点可以是 Element/Text/Comment/Cdata 任意交错
///       （如 `<p>Hello <b>world</b>!</p>`）。属性保插入序 + key 索引，O(1) 查找/设置。
///       命名空间**不特殊处理**：`prefix:local` 整体作为元素名/属性名（不拆分）。
/// @warning 字符串引用生命周期绑定所属 `XmlDocument`：document 析构后引用失效。
///          需要长期持有的用户应自行 clone 出 Utf8String。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"

#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace ca::xml {

/// @brief XML 节点的 4 种形态。
enum class XmlNodeType
{
    Element,   ///< 元素：名字 + 属性 + 子节点
    Text,      ///< 文本内容（实体已解码）
    Comment,   ///< 注释 `<!-- ... -->`（内容不含定界符）
    Cdata      ///< CDATA 段 `<![CDATA[ ... ]]>`（内容原样，不解码实体）
};

/// @brief XML DOM 节点。
/// @details 节点内不拥有字符串内存：元素名、属性名/值、文本/注释/CDATA 内容均为
///          `Utf8StringRef`，指向所属 `XmlDocument` 的 `Utf8StringArena`。
/// @warning 拷贝/移动语义为浅引用：拷贝得到的 XmlNode 的字符串引用仍指向原 arena。
class XmlNode
{
public:
    /// 属性存储：pair 的 first 是属性名，second 是属性值（均 Utf8StringRef）。
    using Attribute        = std::pair<ca::str::Utf8StringRef, ca::str::Utf8StringRef>;
    using AttributeStorage = std::vector<Attribute>;
    /// 子节点存储。
    using ChildStorage = std::vector<XmlNode>;

private:
    /// 元素专属数据：名字 + 属性（保序 + 索引）+ 子节点。
    /// attr_index 的 string_view 指向属性名的 arena 字节（arena 不搬移，vector 扩容不影响），
    /// 只经 set_attribute/remove_attribute 同步修改，不对外暴露可变引用。
    struct ElementData
    {
        ca::str::Utf8StringRef                          name;
        AttributeStorage                                attributes;
        std::unordered_map<std::string_view, ca::usize> attr_index;
        ChildStorage                                    children;
    };

public:
    // ---- 构造 / 析构 / 拷贝 / 移动 ----

    XmlNode() noexcept;                             // 默认构造为空 Text 节点
    XmlNode(const XmlNode&)            = default;   // 浅拷贝（Utf8StringRef 可拷贝）
    XmlNode& operator=(const XmlNode&) = default;
    XmlNode(XmlNode&& other) noexcept;
    XmlNode& operator=(XmlNode&& other) noexcept;
    ~XmlNode();

    /// 显式深拷贝（递归拷贝整个子树）。字符串引用不复制 arena，故深拷贝出的 XmlNode
    /// 仍指向原 arena——只是结构是新的。需完全独立的副本时另起 XmlDocument。
    XmlNode clone() const;

    // ---- 工厂 ----

    static XmlNode make_element(ca::str::Utf8StringRef name);
    static XmlNode make_text(ca::str::Utf8StringRef value) noexcept;
    static XmlNode make_comment(ca::str::Utf8StringRef value) noexcept;
    static XmlNode make_cdata(ca::str::Utf8StringRef value) noexcept;

    // ---- 类型查询 ----

    XmlNodeType type() const noexcept;
    bool        is_element() const noexcept;
    bool        is_text() const noexcept;
    bool        is_comment() const noexcept;
    bool        is_cdata() const noexcept;

    // ---- 元素：名字 ----

    /// @return 元素名。@warning 类型必须为 Element，否则断言失败。
    const ca::str::Utf8StringRef& name() const noexcept;
    /// @brief 设置元素名。@warning 类型必须为 Element，否则断言失败。
    void set_name(ca::str::Utf8StringRef name) noexcept;

    // ---- 元素：属性 ----

    /// @brief 设置属性，覆盖同名。经索引 O(1) 定位。@warning 必须为 Element。
    void set_attribute(ca::str::Utf8StringRef key, ca::str::Utf8StringRef value);
    /// @brief 查属性值，未找到返回 nullptr。O(1)。@warning 必须为 Element。
    const ca::str::Utf8StringRef* attribute(const ca::str::Utf8StringRef& key) const noexcept;
    /// @brief 是否含某属性。@warning 必须为 Element。
    bool has_attribute(const ca::str::Utf8StringRef& key) const noexcept;
    /// @brief 移除属性。@return 存在并被删除返回 true。O(n)（保序删除）。@warning 必须为 Element。
    bool remove_attribute(const ca::str::Utf8StringRef& key) noexcept;
    /// @brief 属性列表（只读，保插入序）。@warning 必须为 Element。
    /// @note 不提供可变引用重载：属性带 key 索引，绕过 set/remove 直接改会破坏一致性。
    const AttributeStorage& attributes() const noexcept;

    // ---- 元素：子节点 ----

    /// @brief 末尾追加子节点。@warning 必须为 Element。
    void append_child(XmlNode child);
    /// @brief 子节点列表（只读）。@warning 必须为 Element。
    const ChildStorage& children() const noexcept;
    /// @brief 子节点列表（可修改，用于批量编辑）。@warning 必须为 Element。
    ChildStorage& children() noexcept;
    /// @brief 子节点数量。@warning 必须为 Element。
    ca::usize child_count() const noexcept;

    /// @brief 按名字查首个**元素**子节点（跳过 text/comment/cdata），未找到返回 nullptr。
    /// @warning 必须为 Element。
    const XmlNode* first_element(const ca::str::Utf8StringRef& name) const noexcept;
    XmlNode*       first_element(const ca::str::Utf8StringRef& name) noexcept;

    /// @brief 拼接元素的**直接** Text 与 Cdata 子节点内容，返回独立 Utf8String。
    /// @details 常用于取「只有文本内容」的元素的值（如 `<port>8080</port>` 取 "8080"）。
    ///          混合内容里穿插的子元素被跳过（只取直接文本）。返回值拥有独立堆内存，
    ///          不受 document 生命周期约束。@warning 必须为 Element。
    ca::str::Utf8String text() const;

    // ---- Text / Comment / Cdata：内容 ----

    /// @return 文本/注释/CDATA 的内容。@warning 类型必须为 Text/Comment/Cdata。
    const ca::str::Utf8StringRef& value() const noexcept;
    /// @brief 设置内容。@warning 类型必须为 Text/Comment/Cdata。
    void set_value(ca::str::Utf8StringRef value) noexcept;

private:
    XmlNodeType type_;
    // Element 用 ElementData；Text/Comment/Cdata 共用 Utf8StringRef（由 type_ 区分）。
    std::variant<ElementData, ca::str::Utf8StringRef> data_;
};

}   // namespace ca::xml
