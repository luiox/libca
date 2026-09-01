#pragma once

/// @file toml_value.hpp
/// @brief TOML DOM 数据模型：TomlValue。
/// @details TomlValue 用一个 `TomlType` 枚举 + 内部 `std::variant` 存储 10 种 TOML 值：
///          String / Integer / Float / Boolean / OffsetDatetime / LocalDateTime / LocalDate /
///          LocalTime / Array / Table。
///          与 json::JsonValue 的关键差异：String 用 `Utf8StringRef`（指向所属 TomlDocument
///          内的 Utf8StringArena），而非 Utf8String。由于 Utf8StringRef 可拷贝，TomlValue
///          整体可拷贝（不再 move-only），builder/push_back 都更简单。
/// @note Table 的 key 也是 Utf8StringRef，intern 入池天然去重。TomlValue 的字符串引用
///       生命周期绑定到所属 TomlDocument：document 析构后引用失效。
///       需要长期持有的用户应自行 clone 出 Utf8String。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/toml/toml_datetime.hpp"

#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace ca::toml {

/// @brief TOML 值的 10 种类型。
enum class TomlType
{
    String,           ///< UTF-8 字符串（Utf8StringRef）
    Integer,          ///< 整数（i64 存储）
    Float,            ///< 浮点（f64 存储）
    Boolean,          ///< 布尔
    OffsetDatetime,   ///< Offset date-time（带时区）
    LocalDateTime,    ///< Local date-time（无时区）
    LocalDate,        ///< Local date（仅年月日）
    LocalTime,        ///< Local time（仅时分秒）
    Array,            ///< 数组（元素任意类型，可嵌套）
    Table             ///< 表（key 为 Utf8StringRef，保序）
};

/// @brief TOML DOM 节点。
/// @details 节点内不拥有字符串内存：String 与 Table key 均为 `Utf8StringRef`，指向所属
///          `TomlDocument` 的 `Utf8StringArena`。TomlValue 因此可拷贝（浅拷贝引用）。
/// @warning 拷贝/移动语义为浅引用：拷贝得到的 TomlValue 的字符串引用仍指向原 arena。
class TomlValue
{
public:
    /// Table 成员的存储类型：pair 的 first 是 key（Utf8StringRef），second 是 value。
    /// @note 用 std::pair 而非自定义 struct 是为了打破 "TomlValue 内含 TomlValue" 的
    ///       循环定义——vector 支持不完整元素类型，pair 由标准库完整定义。
    using TableMember  = std::pair<ca::str::Utf8StringRef, TomlValue>;
    using TableStorage = std::vector<TableMember>;
    /// Array 元素的存储类型。
    using ArrayStorage = std::vector<TomlValue>;

private:
    /// Table 的内部存储：members 保插入序供遍历，index 提供 key → 下标 的 O(1) 查找。
    /// index 的 string_view 指向 key 的 arena 字节（arena 不搬移，成员 vector 扩容不影响）。
    /// 二者只经 set/find/remove 同步修改，不对外暴露可变引用。
    struct TableData
    {
        TableStorage                                    members;
        std::unordered_map<std::string_view, ca::usize> index;
    };

public:
    // ---- 构造 / 析构 / 拷贝 / 移动 ----

    TomlValue() noexcept;                               // 默认构造为 Table（TOML 根）
    TomlValue(const TomlValue&)            = default;   // 浅拷贝（Utf8StringRef 可拷贝）
    TomlValue& operator=(const TomlValue&) = default;
    TomlValue(TomlValue&& other) noexcept;
    TomlValue& operator=(TomlValue&& other) noexcept;
    ~TomlValue();

    /// 显式深拷贝（递归拷贝整个子树）。字符串引用不复制 arena，故深拷贝出的 TomlValue
    /// 仍指向原 arena——只是结构是新的。需完全独立的副本时另起 TomlDocument。
    TomlValue clone() const;

    // ---- 工厂 ----

    static TomlValue make_string(ca::str::Utf8StringRef v) noexcept;
    static TomlValue make_integer(ca::i64 v) noexcept;
    static TomlValue make_float(ca::f64 v) noexcept;
    static TomlValue make_boolean(bool v) noexcept;
    static TomlValue make_offset_datetime(const TomlDatetime& v) noexcept;
    static TomlValue make_local_datetime(const TomlDatetime& v) noexcept;
    static TomlValue make_local_date(const TomlDatetime& v) noexcept;
    static TomlValue make_local_time(const TomlDatetime& v) noexcept;
    static TomlValue make_array();
    static TomlValue make_table();

    // ---- 类型查询 ----

    TomlType type() const noexcept;
    bool     is_string() const noexcept;
    bool     is_integer() const noexcept;
    bool     is_float() const noexcept;
    bool     is_boolean() const noexcept;
    bool     is_offset_datetime() const noexcept;
    bool     is_local_datetime() const noexcept;
    bool     is_local_date() const noexcept;
    bool     is_local_time() const noexcept;
    bool     is_datetime() const noexcept;   // 任一 datetime 变体
    bool     is_array() const noexcept;
    bool     is_table() const noexcept;

    // ---- 严格访问（类型不符触发断言） ----

    /// @return string 引用。@warning 类型必须为 String，否则断言失败。
    const ca::str::Utf8StringRef& as_string() const noexcept;
    /// @return int 值。@warning 类型必须为 Integer，否则断言失败。
    ca::i64 as_integer() const noexcept;
    /// @return float 值。@warning 类型必须为 Float，否则断言失败。
    ca::f64 as_float() const noexcept;
    /// @return bool 值。@warning 类型必须为 Boolean，否则断言失败。
    bool as_boolean() const noexcept;
    /// @return OffsetDatetime 引用。@warning 类型必须为 OffsetDatetime，否则断言失败。
    const TomlDatetime& as_offset_datetime() const noexcept;
    /// @return LocalDateTime 引用。@warning 类型必须为 LocalDateTime，否则断言失败。
    const TomlDatetime& as_local_datetime() const noexcept;
    /// @return LocalDate 引用。@warning 类型必须为 LocalDate，否则断言失败。
    const TomlDatetime& as_local_date() const noexcept;
    /// @return LocalTime 引用。@warning 类型必须为 LocalTime，否则断言失败。
    const TomlDatetime& as_local_time() const noexcept;
    /// @return 任意 datetime 变体的引用。@warning 必须是 4 种 datetime 之一，否则断言失败。
    const TomlDatetime& as_datetime() const noexcept;
    /// @return array 引用。@warning 类型必须为 Array，否则断言失败。
    const ArrayStorage& as_array() const noexcept;
    /// @return array 引用（可修改）。@warning 类型必须为 Array，否则断言失败。
    ArrayStorage& as_array() noexcept;
    /// @return table 成员列表（只读，保插入序）。@warning 类型必须为 Table，否则断言失败。
    /// @note 不提供可变引用重载：Table 内部带 key 索引，绕过 set/remove 直接改成员
    ///       列表会破坏索引一致性。
    const TableStorage& as_table() const noexcept;

    // ---- 数值安全互转 ----

    /// @brief 取浮点值；Integer 自动转 float，其它类型返回 fallback。
    ca::f64 as_float_or(ca::f64 fallback) const noexcept;
    /// @brief 取整数值；Float 截断为 int，其它类型返回 fallback。
    ca::i64 as_integer_or(ca::i64 fallback) const noexcept;

    // ---- Array 编辑 ----

    /// @brief 数组末尾追加一个值。@warning 类型必须为 Array，否则断言失败。
    void append(TomlValue v);

    /// @brief 取数组元素（只读）。@warning 类型必须为 Array，index 越界为 UB。
    const TomlValue& at(ca::usize index) const noexcept;

    /// @brief 取数组元素（可修改）。@warning 类型必须为 Array，index 越界为 UB。
    TomlValue& at(ca::usize index) noexcept;

    /// @brief 数组元素数量。@warning 类型必须为 Array，否则断言失败。
    ca::usize size() const noexcept;

    // ---- Table 编辑 ----

    /// @brief 设置 key 的值，覆盖同名 key。经索引 O(1) 定位。
    /// @warning 类型必须为 Table，否则断言失败。
    void set(ca::str::Utf8StringRef key, TomlValue v);

    /// @brief 查找 key（只读）。未找到返回 nullptr。经索引 O(1) 定位。
    /// @warning 类型必须为 Table，否则断言失败。
    const TomlValue* find(const ca::str::Utf8StringRef& key) const noexcept;

    /// @brief 查找 key（可修改）。未找到返回 nullptr。经索引 O(1) 定位。
    /// @warning 类型必须为 Table，否则断言失败。
    TomlValue* find(const ca::str::Utf8StringRef& key) noexcept;

    /// @brief 移除 key。@return key 存在并被删除时返回 true。O(n)（成员保序删除）。
    /// @warning 类型必须为 Table，否则断言失败。
    bool remove(const ca::str::Utf8StringRef& key) noexcept;

private:
    TomlType type_;
    std::variant<std::monostate,           ///< Null（内部用，TOML 无 null）
                 ca::str::Utf8StringRef,   ///< String
                 ca::i64,                  ///< Integer
                 ca::f64,                  ///< Float
                 bool,                     ///< Boolean
                 TomlDatetime,             ///< 4 种 datetime 变体（Kind 区分）
                 ArrayStorage,             ///< Array
                 TableData>
        data_;   ///< Table
};

}   // namespace ca::toml
