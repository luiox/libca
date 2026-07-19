#pragma once

/// @file json_value.hpp
/// @brief JSON DOM 数据模型：JsonValue。
/// @details JsonValue 用一个 `JsonType` 枚举 + 内部 `std::variant` 存储七种 JSON 值：
///          null / bool / int / float / string / array / object。
///          number 区分 int(i64) 与 float(f64)：字面量不含 `.`/`e`/`E` 时按 int 解析，
///          超过 i64 范围自动降级为 float。String 用 Utf8StringRef（指向所属 JsonDocument
///          内的 Utf8StringArena），Array 用 vector<JsonValue>，
///          Object 用 vector<pair<Utf8StringRef, JsonValue>>（保序、允许重复 key，set() 覆盖同名 key）。
/// @note 由于 Utf8StringRef 可拷贝，JsonValue 整体可拷贝（不再 move-only），
///       builder/push_back 都更简单。
///       Object 的 key 也是 Utf8StringRef，intern 入池天然去重。
///       JsonValue 的字符串引用生命周期绑定到所属 JsonDocument：document 析构后引用失效。
///       需要长期持有的用户应自行 clone 出 Utf8String。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"

#include <utility>
#include <variant>
#include <vector>

namespace ca::json {

/// @brief JSON 值的七种类型。
enum class JsonType {
    Null,    ///< null
    Bool,    ///< true / false
    Int,     ///< 整数（i64 存储）
    Float,   ///< 浮点（f64 存储）
    String,  ///< UTF-8 字符串（Utf8StringRef）
    Array,   ///< 数组
    Object   ///< 对象（键值对，键为 Utf8StringRef）
};

/// @brief JSON DOM 节点。
/// @details 节点内不拥有字符串内存：String 与 Object key 均为 `Utf8StringRef`，指向所属
///          `JsonDocument` 的 `Utf8StringArena`。JsonValue 因此可拷贝（浅拷贝引用）。
/// @warning 拷贝/移动语义为浅引用：拷贝得到的 JsonValue 的字符串引用仍指向原 arena。
class JsonValue {
public:
    /// Object 成员的存储类型：pair 的 first 是 key（Utf8StringRef），second 是 value。
    /// @note 用 std::pair 而非自定义 struct 是为了打破 "JsonValue 内含 JsonValue" 的
    ///       循环定义——vector 支持不完整元素类型，pair 由标准库完整定义。
    using ObjectMember  = std::pair<ca::str::Utf8StringRef, JsonValue>;
    using ObjectStorage = std::vector<ObjectMember>;
    /// Array 元素的存储类型。
    using ArrayStorage  = std::vector<JsonValue>;

    // ---- 构造 / 析构 / 拷贝 / 移动 ----

    JsonValue() noexcept;                             // 默认构造为 null
    JsonValue(const JsonValue&) = default;            // 浅拷贝（Utf8StringRef 可拷贝）
    JsonValue& operator=(const JsonValue&) = default;
    JsonValue(JsonValue&& other) noexcept;
    JsonValue& operator=(JsonValue&& other) noexcept;
    ~JsonValue();

    /// 显式深拷贝（递归拷贝整个子树）。字符串引用不复制 arena，故深拷贝出的 JsonValue
    /// 仍指向原 arena——只是结构是新的。需完全独立的副本时另起 JsonDocument。
    JsonValue clone() const;

    // ---- 工厂 ----

    static JsonValue make_null() noexcept;
    static JsonValue make_bool(bool v) noexcept;
    static JsonValue make_int(ca::i64 v) noexcept;
    static JsonValue make_float(ca::f64 v) noexcept;
    static JsonValue make_string(ca::str::Utf8StringRef v) noexcept;
    static JsonValue make_array();
    static JsonValue make_object();

    // ---- 类型查询 ----

    JsonType type() const noexcept;
    bool is_null() const noexcept;
    bool is_bool() const noexcept;
    bool is_int() const noexcept;
    bool is_float() const noexcept;
    bool is_number() const noexcept;   // Int 或 Float
    bool is_string() const noexcept;
    bool is_array() const noexcept;
    bool is_object() const noexcept;

    // ---- 严格访问（类型不符触发断言） ----

    /// @return bool 值。@warning 类型必须为 Bool，否则断言失败。
    bool as_bool() const noexcept;
    /// @return int 值。@warning 类型必须为 Int，否则断言失败。
    ca::i64 as_int() const noexcept;
    /// @return float 值。@warning 类型必须为 Float，否则断言失败。
    ca::f64 as_float() const noexcept;
    /// @return string 引用。@warning 类型必须为 String，否则断言失败。
    const ca::str::Utf8StringRef& as_string() const noexcept;
    /// @return array 引用。@warning 类型必须为 Array，否则断言失败。
    const ArrayStorage& as_array() const noexcept;
    /// @return object 引用。@warning 类型必须为 Object，否则断言失败。
    const ObjectStorage& as_object() const noexcept;

    // ---- 数值安全互转 ----

    /// @brief 取浮点值；Int 自动转 float，其它类型返回 fallback。
    ca::f64 as_float_or(ca::f64 fallback) const noexcept;
    /// @brief 取整数值；Float 截断为 int，其它类型返回 fallback。
    ca::i64 as_int_or(ca::i64 fallback) const noexcept;

    // ---- Array 编辑 ----

    /// @brief 数组末尾追加一个值。
    /// @warning 类型必须为 Array，否则断言失败。
    void append(JsonValue v);

    /// @brief 取数组元素（只读）。@warning 类型必须为 Array，index 越界为 UB。
    const JsonValue& at(ca::usize index) const noexcept;

    /// @brief 取数组元素（可修改）。@warning 类型必须为 Array，index 越界为 UB。
    JsonValue& at(ca::usize index) noexcept;

    /// @brief 数组元素数量。@warning 类型必须为 Array，否则断言失败。
    ca::usize size() const noexcept;

    // ---- Object 编辑 ----

    /// @brief 设置 key 的值，覆盖同名 key。
    /// @warning 类型必须为 Object，否则断言失败。
    void set(ca::str::Utf8StringRef key, JsonValue v);

    /// @brief 查找 key（只读）。未找到返回 nullptr。
    /// @warning 类型必须为 Object，否则断言失败。
    const JsonValue* find(const ca::str::Utf8StringRef& key) const noexcept;

    /// @brief 查找 key（可修改）。未找到返回 nullptr。
    /// @warning 类型必须为 Object，否则断言失败。
    JsonValue* find(const ca::str::Utf8StringRef& key) noexcept;

    /// @brief 移除 key。@return 实际移除时返回 true。
    /// @warning 类型必须为 Object，否则断言失败。
    bool remove(const ca::str::Utf8StringRef& key) noexcept;

private:
    JsonType type_;
    std::variant<std::monostate,
                 bool,
                 ca::i64,
                 ca::f64,
                 ca::str::Utf8StringRef,
                 ArrayStorage,
                 ObjectStorage> data_;
};

}  // namespace ca::json
