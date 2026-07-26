#pragma once

/// @file yaml_value.hpp
/// @brief YAML DOM 数据模型：YamlValue。
/// @details YamlValue 用一个 `YamlType` 枚举 + 内部 `std::variant` 存储 7 种 YAML 值：
///          Null / Boolean / Integer / Float / String / Sequence / Mapping。
///          与 toml::TomlValue 同构：String 用 `Utf8StringRef`（指向所属 YamlDocument
///          内的 Utf8StringArena），YamlValue 整体可拷贝（浅拷贝引用）。
/// @note 与 TOML 的关键差异：Null 是 YAML 的真实类型（`~`/`null`/空值），默认构造
///       即 Null（YAML 根可为任意节点，而 TOML 根固定是 Table）。
///       Mapping 的 key 也是 Utf8StringRef，intern 入池天然去重。YamlValue 的字符串引用
///       生命周期绑定到所属 YamlDocument：document 析构后引用失效。
///       需要长期持有的用户应自行 clone 出 Utf8String。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"

#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace ca::yaml {

/// @brief YAML 值的 7 种类型。
enum class YamlType {
    Null,      ///< null / ~ / 空值
    Boolean,   ///< 布尔（true/false）
    Integer,   ///< 整数（i64 存储）
    Float,     ///< 浮点（f64 存储）
    String,    ///< UTF-8 字符串（Utf8StringRef）
    Sequence,  ///< 序列（元素任意类型，可嵌套）
    Mapping    ///< 映射（key 为 Utf8StringRef，保序）
};

/// @brief YAML DOM 节点。
/// @details 节点内不拥有字符串内存：String 与 Mapping key 均为 `Utf8StringRef`，指向所属
///          `YamlDocument` 的 `Utf8StringArena`。YamlValue 因此可拷贝（浅拷贝引用）。
/// @warning 拷贝/移动语义为浅引用：拷贝得到的 YamlValue 的字符串引用仍指向原 arena。
class YamlValue {
public:
    /// Mapping 成员的存储类型：pair 的 first 是 key（Utf8StringRef），second 是 value。
    /// @note 用 std::pair 而非自定义 struct 是为了打破 "YamlValue 内含 YamlValue" 的
    ///       循环定义——vector 支持不完整元素类型，pair 由标准库完整定义。
    using MappingMember  = std::pair<ca::str::Utf8StringRef, YamlValue>;
    using MappingStorage = std::vector<MappingMember>;
    /// Sequence 元素的存储类型。
    using SequenceStorage = std::vector<YamlValue>;

private:
    /// Mapping 的内部存储：members 保插入序供遍历，index 提供 key → 下标 的 O(1) 查找。
    /// index 的 string_view 指向 key 的 arena 字节（arena 不搬移，成员 vector 扩容不影响）。
    /// 二者只经 set/find/remove 同步修改，不对外暴露可变引用。
    struct MappingData {
        MappingStorage members;
        std::unordered_map<std::string_view, ca::usize> index;
    };

public:
    // ---- 构造 / 析构 / 拷贝 / 移动 ----

    YamlValue() noexcept;                             // 默认构造为 Null
    YamlValue(const YamlValue&) = default;            // 浅拷贝（Utf8StringRef 可拷贝）
    YamlValue& operator=(const YamlValue&) = default;
    YamlValue(YamlValue&& other) noexcept;
    YamlValue& operator=(YamlValue&& other) noexcept;
    ~YamlValue();

    /// 显式深拷贝（递归拷贝整个子树）。字符串引用不复制 arena，故深拷贝出的 YamlValue
    /// 仍指向原 arena——只是结构是新的。需完全独立的副本时另起 YamlDocument。
    YamlValue clone() const;

    // ---- 工厂 ----

    static YamlValue make_null() noexcept;
    static YamlValue make_boolean(bool v) noexcept;
    static YamlValue make_integer(ca::i64 v) noexcept;
    static YamlValue make_float(ca::f64 v) noexcept;
    static YamlValue make_string(ca::str::Utf8StringRef v) noexcept;
    static YamlValue make_sequence();
    static YamlValue make_mapping();

    // ---- 类型查询 ----

    YamlType type() const noexcept;
    bool is_null() const noexcept;
    bool is_boolean() const noexcept;
    bool is_integer() const noexcept;
    bool is_float() const noexcept;
    bool is_string() const noexcept;
    bool is_sequence() const noexcept;
    bool is_mapping() const noexcept;

    // ---- 严格访问（类型不符触发断言） ----

    /// @return bool 值。@warning 类型必须为 Boolean，否则断言失败。
    bool as_boolean() const noexcept;
    /// @return int 值。@warning 类型必须为 Integer，否则断言失败。
    ca::i64 as_integer() const noexcept;
    /// @return float 值。@warning 类型必须为 Float，否则断言失败。
    ca::f64 as_float() const noexcept;
    /// @return string 引用。@warning 类型必须为 String，否则断言失败。
    const ca::str::Utf8StringRef& as_string() const noexcept;
    /// @return sequence 引用。@warning 类型必须为 Sequence，否则断言失败。
    const SequenceStorage& as_sequence() const noexcept;
    /// @return sequence 引用（可修改）。@warning 类型必须为 Sequence，否则断言失败。
    SequenceStorage& as_sequence() noexcept;
    /// @return mapping 成员列表（只读，保插入序）。@warning 类型必须为 Mapping，否则断言失败。
    /// @note 不提供可变引用重载：Mapping 内部带 key 索引，绕过 set/remove 直接改成员
    ///       列表会破坏索引一致性。
    const MappingStorage& as_mapping() const noexcept;

    // ---- 数值安全互转 ----

    /// @brief 取浮点值；Integer 自动转 float，其它类型返回 fallback。
    ca::f64 as_float_or(ca::f64 fallback) const noexcept;
    /// @brief 取整数值；Float 截断为 int，其它类型返回 fallback。
    ca::i64 as_integer_or(ca::i64 fallback) const noexcept;

    // ---- Sequence 编辑 ----

    /// @brief 序列末尾追加一个值。@warning 类型必须为 Sequence，否则断言失败。
    void append(YamlValue v);

    /// @brief 取序列元素（只读）。@warning 类型必须为 Sequence，index 越界为 UB。
    const YamlValue& at(ca::usize index) const noexcept;

    /// @brief 取序列元素（可修改）。@warning 类型必须为 Sequence，index 越界为 UB。
    YamlValue& at(ca::usize index) noexcept;

    /// @brief 序列元素数量。@warning 类型必须为 Sequence，否则断言失败。
    ca::usize size() const noexcept;

    // ---- Mapping 编辑 ----

    /// @brief 设置 key 的值，覆盖同名 key。经索引 O(1) 定位。
    /// @warning 类型必须为 Mapping，否则断言失败。
    void set(ca::str::Utf8StringRef key, YamlValue v);

    /// @brief 查找 key（只读）。未找到返回 nullptr。经索引 O(1) 定位。
    /// @warning 类型必须为 Mapping，否则断言失败。
    const YamlValue* find(const ca::str::Utf8StringRef& key) const noexcept;

    /// @brief 查找 key（可修改）。未找到返回 nullptr。经索引 O(1) 定位。
    /// @warning 类型必须为 Mapping，否则断言失败。
    YamlValue* find(const ca::str::Utf8StringRef& key) noexcept;

    /// @brief 移除 key。@return key 存在并被删除时返回 true。O(n)（成员保序删除）。
    /// @warning 类型必须为 Mapping，否则断言失败。
    bool remove(const ca::str::Utf8StringRef& key) noexcept;

private:
    YamlType type_;
    std::variant<std::monostate,                ///< Null
                 bool,                           ///< Boolean
                 ca::i64,                        ///< Integer
                 ca::f64,                        ///< Float
                 ca::str::Utf8StringRef,         ///< String
                 SequenceStorage,                ///< Sequence
                 MappingData> data_;             ///< Mapping
};

}  // namespace ca::yaml
