#pragma once

/// @file config_var.hpp
/// @brief 配置项抽象基类 ConfigVarBase 与模板实现 ConfigVar<T>。
/// @details ConfigVar<T> 持有一个强类型配置值：支持带"值相等短路"的写入、
///          变更监听器（锁外回调）以及与 JSON 的双向转换。
///          T 的约束：可默认构造、可拷贝、支持 operator==。

#pragma once

#include "libca/config/config_error.hpp"
#include "libca/config/lexical_cast.hpp"

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/json/json_document.hpp"
#include "libca/json/json_value.hpp"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

namespace ca::config {

/// @brief 配置项抽象基类：Config 注册中心只面向该接口操作。
class ConfigVarBase {
public:
    /// @param name 配置项名（即 JSON 顶层 key）。
    /// @param description 人读描述，可为空。
    ConfigVarBase(std::string name, std::string description)
        : name_(std::move(name)), description_(std::move(description))
    {
    }

    virtual ~ConfigVarBase() = default;

    ConfigVarBase(const ConfigVarBase&) = delete;
    ConfigVarBase& operator=(const ConfigVarBase&) = delete;

    /// @return 配置项名。
    const std::string& name() const noexcept
    {
        return name_;
    }

    /// @return 人读描述。
    const std::string& description() const noexcept
    {
        return description_;
    }

    /// @return 配置项的具体类型（typeid(T)），Config 用它做类型冲突检测。
    virtual std::type_index type() const noexcept = 0;

    /// @brief 序列化当前值为 JSON。
    /// @param document 调用方提供的文档：字符串 intern 到 document.arena()，
    ///                 返回的 JsonValue 生命周期跟随该 document。
    /// @note 不提供无参便捷版本：对象内嵌去重池会让每个配置项常驻一块
    ///       arena 内存（且历史值只增不减），短命序列化结果不值得这个代价。
    virtual ca::json::JsonValue to_json(ca::json::JsonDocument& document) const = 0;

    /// @brief 类型校验并从 JSON 赋值（内部走 set，值相等短路、监听器锁外触发）。
    /// @param value JSON 值。
    /// @return 类型不符/超范围返回对应 ConfigError；成功返回 Ok。
    virtual Result<void, ConfigError> set_from_json(const ca::json::JsonValue& value) = 0;

private:
    std::string name_;
    std::string description_;
};

/// @brief 强类型配置项。
/// @tparam T 值类型：需可默认构造、可拷贝、支持 operator==
///         （bool / 整型 / 浮点 / std::string / 递归容器均可，见 JsonCast<T>）。
template <typename T>
class ConfigVar : public ConfigVarBase {
public:
    /// 值变更监听器：old_value 为旧值，new_value 为新值。
    using Listener = std::function<void(const T& old_value, const T& new_value)>;

    /// @param name 配置项名。@param default_value 默认值。@param description 人读描述。
    ConfigVar(std::string name, const T& default_value, std::string description = "")
        : ConfigVarBase(std::move(name), std::move(description)), value_(default_value)
    {
    }

    /// @return 配置项类型（typeid(T)）。
    std::type_index type() const noexcept override
    {
        return typeid(T);
    }

    /// @brief 取当前值。
    /// @return 值的拷贝（内部锁保护，并发 set 下返回完整一致快照）。
    T value() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return value_;
    }

    /// @brief 写入新值。
    /// @param value 新值。
    /// @return 值真实变化返回 true；与旧值相等（operator==）时短路，返回 false，
    ///         不换值、不触发监听器。
    /// @note 监听器在本函数内部锁之外触发：回调中可安全再进 Config 或本对象。
    bool set(const T& value)
    {
        std::vector<Listener> pending;
        T old_value;
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            if (value_ == value) {
                return false;
            }
            old_value = value_;
            value_ = value;
            pending.reserve(listeners_.size());
            for (const auto& entry : listeners_) {
                pending.push_back(entry.second);
            }
        }
        // 锁外触发：拷贝监听器列表后释放内部锁，回调不会阻塞其它线程的读写
        for (const auto& listener : pending) {
            if (listener) {
                listener(old_value, value);
            }
        }
        return true;
    }

    /// @brief 注册值变更监听器。@return 监听器 id（0 保留为无效，首个返回 1）。
    /// @note 只在值真实变化时被调用；重复添加同一回调会得到多个独立 id。
    ca::u64 add_listener(Listener listener)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        const ca::u64 id = ++next_listener_id_;
        listeners_.emplace(id, std::move(listener));
        return id;
    }

    /// @brief 移除监听器。
    /// @param id add_listener 返回的 id。
    /// @return 存在并已移除返回 true；id 不存在（含 0）返回 false。
    bool remove_listener(ca::u64 id)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return listeners_.erase(id) > 0;
    }

    /// @brief 序列化当前值为 JSON，字符串 intern 到调用方 document。
    ca::json::JsonValue to_json(ca::json::JsonDocument& document) const override
    {
        // 共享锁内拷贝值快照，锁外装配 JSON
        T snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            snapshot = value_;
        }
        return JsonCast<T>::to_json(snapshot, document.arena());
    }

    /// @brief 类型校验并从 JSON 赋值（值相等短路、监听器锁外触发）。
    Result<void, ConfigError> set_from_json(const ca::json::JsonValue& value) override
    {
        auto converted = JsonCast<T>::from_json(value);
        if (converted.is_err()) {
            return Err(std::move(converted).unwrap_err());
        }
        set(std::move(converted).unwrap());
        return Ok();
    }

private:
    /// 内部锁：保护 value_ / listeners_ / next_listener_id_。
    mutable std::shared_mutex mutex_;
    T value_;
    /// 监听器表：id → 回调（有序表便于稳定遍历）。
    std::map<ca::u64, Listener> listeners_;
    /// 监听器 id 发生器，0 保留为无效。
    ca::u64 next_listener_id_ = 0;
};

}  // namespace ca::config
