#pragma once

/// @file config.hpp
/// @brief Config 静态注册中心：按名字查找/注册强类型配置项 ConfigVar<T>。
/// @details sylar 风格配置中心的 libca 适配：
///          - lookup<T>(name, default, description) 幂等：不存在则创建注册；
///            已存在且类型一致返回现有实例（忽略本次 default）；类型冲突返回 nullptr；
///          - lookup 是模板，定义在本头文件；其余接口在 config.cpp。

#include "libca/config/config_var.hpp"

#include "libca/core/datatype.hpp"

#include "libca/json/json_document.hpp"

#include <memory>
#include <shared_mutex>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace ca::config {

namespace detail {

/// @brief Config 全局注册表状态（内部实现细节，非公开接口）。
struct ConfigState {
    /// 注册表锁：lookup 写注册表用独占锁，只读查询/遍历用共享锁。
    std::shared_mutex mutex;
    /// 已注册的配置项：name → var。
    std::unordered_map<std::string, std::shared_ptr<ConfigVarBase>> vars;
};

/// @brief 全局单例状态（函数级 static，规避静态初始化顺序问题）。
ConfigState& config_state();

}  // namespace ca::config::detail

/// @brief 配置注册中心（纯静态接口，不可实例化）。
class Config {
public:
    Config() = delete;

    /// @brief 查找/创建强类型配置项（幂等）。
    /// @tparam T 值类型，见 ConfigVar<T> 的类型约束。
    /// @param name 配置项名。
    /// @param default_value 首次创建时的默认值；已存在时忽略。
    /// @param description 人读描述；已存在时忽略。
    /// @return 现有或新建的 ConfigVar<T>；name 已存在但类型冲突（type_index 不一致）
    ///         返回 nullptr。
    /// @note 线程安全：与 load / visit / 其它 lookup 可并发调用。
    template <typename T>
    static std::shared_ptr<ConfigVar<T>> lookup(const std::string& name, const T& default_value,
                                                const std::string& description = "")
    {
        detail::ConfigState& state = detail::config_state();
        std::unique_lock<std::shared_mutex> lock(state.mutex);

        // 已注册：类型一致 → 幂等返回现有实例（忽略 default）；不一致 → 类型冲突
        const auto it = state.vars.find(name);
        if (it != state.vars.end()) {
            if (it->second->type() != std::type_index(typeid(T))) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
        }

        auto var = std::make_shared<ConfigVar<T>>(name, default_value, description);
        state.vars.emplace(name, var);
        return var;
    }

    /// @brief 按名字查找配置项（类型擦除视图）。
    /// @param name 配置项名。
    /// @return 已注册返回 ConfigVarBase；不存在返回 nullptr。
    static std::shared_ptr<ConfigVarBase> lookup_base(const std::string& name);

    /// @brief 清空注册表（含全部已注册配置项）。
    /// @note 仅供测试与程序初始化阶段重建注册表使用；已逃逸出去的
    ///       shared_ptr<ConfigVar<T>> 仍有效，但不再受注册中心管理。
    static void clear();
};

}  // namespace ca::config
