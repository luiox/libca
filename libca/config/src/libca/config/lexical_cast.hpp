#pragma once

/// @file lexical_cast.hpp
/// @brief JsonValue 与 C++ 类型的双向转换链 JsonCast<T>（header-only）。
/// @details 正向 from_json(JsonValue) → Result<T, ConfigError>（严格类型校验 + 整型范围检查），
///          反向 to_json(T, arena) → JsonValue（字符串 intern 到调用方 arena）。
///          支持标量 bool / std::string / 整型（含 ca::i32 等） / 浮点，以及
///          std::vector<T>、std::unordered_map<std::string, T> 的递归容器转换。

#include "libca/config/config_error.hpp"

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/json/json_value.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace ca::config {

namespace detail {

/// 依赖模板参数的恒 false，供主模板 static_assert 使用（规避 MSVC 对恒假断言的误报）。
template <typename T>
struct dependent_false : std::false_type {
};

}  // namespace ca::config::detail

/// @brief JsonValue 与 T 的双向转换 Traits。
/// @tparam T 目标 C++ 类型。
/// @note 主模板不提供实现：未支持的 T 在实例化处得到明确的编译错误。
///       from_json 语义约定：
///       - bool 只接受 JSON Bool；
///       - 整型只接受 JSON Int，超出目标类型范围报 OUT_OF_RANGE；
///       - 浮点接受 JSON Int 或 Float；
///       - std::string 只接受 JSON String；
///       - 容器递归转换，任一元素失败则整体失败。
template <typename T, typename Enable = void>
struct JsonCast {
    static_assert(detail::dependent_false<T>::value,
                  "JsonCast<T>: 该类型未提供 JSON 转换，请在配置中使用受支持的类型");
};

/// @brief bool 转换：JSON Bool ↔ bool。
template <>
struct JsonCast<bool> {
    /// @brief 从 JSON 读取 bool。@return 类型不符返回 TYPE_MISMATCH。
    static Result<bool, ConfigError> from_json(const ca::json::JsonValue& value)
    {
        if (!value.is_bool()) {
            return Err(ConfigError::TYPE_MISMATCH);
        }
        return Ok(value.as_bool());
    }

    /// @brief bool 序列化为 JSON Bool。
    static ca::json::JsonValue to_json(const bool& value, ca::str::Utf8StringArena& arena)
    {
        (void)arena;
        return ca::json::JsonValue::make_bool(value);
    }
};

/// @brief std::string 转换：JSON String ↔ std::string。
template <>
struct JsonCast<std::string> {
    /// @brief 从 JSON 读取字符串。@return 类型不符返回 TYPE_MISMATCH。
    static Result<std::string, ConfigError> from_json(const ca::json::JsonValue& value)
    {
        if (!value.is_string()) {
            return Err(ConfigError::TYPE_MISMATCH);
        }
        return Ok(value.as_string().to_std_string());
    }

    /// @brief 字符串序列化为 JSON String（intern 入 arena）。
    static ca::json::JsonValue to_json(const std::string& value, ca::str::Utf8StringArena& arena)
    {
        return ca::json::JsonValue::make_string(arena.intern(value));
    }
};

/// @brief 整型转换：JSON Int ↔ 整型（bool 除外），带目标类型范围检查。
/// @note JSON Int 以 i64 存储；u64 的超出 i64 正域部分在解析期已降级为 Float，
///       对本转换表现为类型不符（受 JSON 表示能力限制，见模块设计文档）。
template <typename T>
struct JsonCast<T, std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value>> {
    /// @brief 从 JSON 读取整型并做范围检查。
    /// @return 类型不符返回 TYPE_MISMATCH；超范围返回 OUT_OF_RANGE。
    static Result<T, ConfigError> from_json(const ca::json::JsonValue& value)
    {
        if (!value.is_int()) {
            return Err(ConfigError::TYPE_MISMATCH);
        }
        const ca::i64 raw = value.as_int();
        bool in_range = false;
        if constexpr (std::is_signed<T>::value) {
            in_range = raw >= static_cast<ca::i64>(std::numeric_limits<T>::min())
                       && raw <= static_cast<ca::i64>(std::numeric_limits<T>::max());
        } else {
            // 无符号：负数直接越界；T 宽度不小于 i64 时任何非负 i64 都可容纳
            //（避免 u64 max 转 i64 的实现定义回绕）。
            in_range = raw >= 0
                       && (sizeof(T) >= sizeof(ca::i64)
                           || raw <= static_cast<ca::i64>(std::numeric_limits<T>::max()));
        }
        if (!in_range) {
            return Err(ConfigError::OUT_OF_RANGE);
        }
        return Ok(static_cast<T>(raw));
    }

    /// @brief 整型序列化为 JSON Int。
    /// @note u64 高于 i64 正域的值 JSON Int 装不下：降级为 Float 承载（与解析期
    ///       该域降级 Float 的行为对称），避免 static_cast 回绕成负数造成静默值损坏。
    static ca::json::JsonValue to_json(const T& value, ca::str::Utf8StringArena& arena)
    {
        (void)arena;
        if constexpr (sizeof(T) == sizeof(ca::i64) && !std::is_signed<T>::value) {
            if (value > static_cast<T>(std::numeric_limits<ca::i64>::max())) {
                return ca::json::JsonValue::make_float(static_cast<ca::f64>(value));
            }
        }
        return ca::json::JsonValue::make_int(static_cast<ca::i64>(value));
    }
};

/// @brief 浮点转换：JSON Int 或 Float ↔ 浮点。
template <typename T>
struct JsonCast<T, std::enable_if_t<std::is_floating_point<T>::value>> {
    /// @brief 从 JSON 读取浮点；Int 自动提升。@return 其它类型返回 TYPE_MISMATCH。
    static Result<T, ConfigError> from_json(const ca::json::JsonValue& value)
    {
        if (value.is_float()) {
            return Ok(static_cast<T>(value.as_float()));
        }
        if (value.is_int()) {
            return Ok(static_cast<T>(value.as_int()));
        }
        return Err(ConfigError::TYPE_MISMATCH);
    }

    /// @brief 浮点序列化为 JSON Float。
    static ca::json::JsonValue to_json(const T& value, ca::str::Utf8StringArena& arena)
    {
        (void)arena;
        return ca::json::JsonValue::make_float(static_cast<ca::f64>(value));
    }
};

/// @brief std::vector<T> 转换：JSON Array ↔ vector，元素递归转换。
template <typename T>
struct JsonCast<std::vector<T>> {
    /// @brief 从 JSON 数组读取。@return 数组内任一元素失败则整体失败并透传元素错误。
    static Result<std::vector<T>, ConfigError> from_json(const ca::json::JsonValue& value)
    {
        if (!value.is_array()) {
            return Err(ConfigError::TYPE_MISMATCH);
        }
        std::vector<T> out;
        out.reserve(value.size());
        for (const auto& item : value.as_array()) {
            auto converted = JsonCast<T>::from_json(item);
            if (converted.is_err()) {
                return Err(std::move(converted).unwrap_err());
            }
            out.push_back(std::move(converted).unwrap());
        }
        return Ok(std::move(out));
    }

    /// @brief vector 序列化为 JSON Array。
    static ca::json::JsonValue to_json(const std::vector<T>& values, ca::str::Utf8StringArena& arena)
    {
        ca::json::JsonValue array = ca::json::JsonValue::make_array();
        for (const auto& item : values) {
            array.append(JsonCast<T>::to_json(item, arena));
        }
        return array;
    }
};

/// @brief std::unordered_map<std::string, T> 转换：JSON Object ↔ map，值递归转换。
template <typename T>
struct JsonCast<std::unordered_map<std::string, T>> {
    using MapType = std::unordered_map<std::string, T>;

    /// @brief 从 JSON 对象读取。
    /// @return 值转换整体失败透传；JSON 重复 key 时后者覆盖前者（与 unordered_map 语义一致）。
    static Result<MapType, ConfigError> from_json(const ca::json::JsonValue& value)
    {
        if (!value.is_object()) {
            return Err(ConfigError::TYPE_MISMATCH);
        }
        MapType out;
        for (const auto& member : value.as_object()) {
            auto converted = JsonCast<T>::from_json(member.second);
            if (converted.is_err()) {
                return Err(std::move(converted).unwrap_err());
            }
            out[member.first.to_std_string()] = std::move(converted).unwrap();
        }
        return Ok(std::move(out));
    }

    /// @brief map 序列化为 JSON Object（key intern 入 arena）。
    static ca::json::JsonValue to_json(const MapType& values, ca::str::Utf8StringArena& arena)
    {
        ca::json::JsonValue object = ca::json::JsonValue::make_object();
        for (const auto& entry : values) {
            object.set(arena.intern(entry.first), JsonCast<T>::to_json(entry.second, arena));
        }
        return object;
    }
};

}  // namespace ca::config
