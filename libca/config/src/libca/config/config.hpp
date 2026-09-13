#pragma once

/// @file config.hpp
/// @brief Config 静态注册中心：按名字查找/注册强类型配置项 ConfigVar<T>，并从 JSON 加载。
/// @details sylar 风格配置中心的 libca 适配：
///          - lookup<T>(name, default, description) 幂等：不存在则创建注册；
///            已存在且类型一致返回现有实例（忽略本次 default）；类型冲突返回 nullptr；
///          - load 文本加载：顶层必须为 JSON object；已注册的 key 直接应用到对应 var；
///            尚无人认领的 key 存入"未物化"表，之后 lookup<T> 命中该 key 时用已加载值
///            物化（转换失败/类型不符退回 default）；
///          - lookup 是模板，定义在本头文件；其余接口在 config.cpp。

#include "libca/config/config_error.hpp"
#include "libca/config/config_var.hpp"

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/json/json_document.hpp"
#include "libca/str/utf8_string.hpp"

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace ca::config {

namespace detail {

/// @brief Config 全局注册表状态（内部实现细节，非公开接口）。
struct ConfigState {
    /// 注册表锁：写（注册/物化/load 应用/清空）用独占锁，只读查询/遍历用共享锁。
    std::shared_mutex mutex;
    /// 已注册的配置项：name → var（含已物化条目，只增不清，clear 除外）。
    std::unordered_map<std::string, std::shared_ptr<ConfigVarBase>> vars;
    /// 未物化的已加载 key：name → 所属文档（root 即整份配置对象，保活其中的字符串）。
    std::unordered_map<std::string, std::shared_ptr<ca::json::JsonDocument>> pending;
};

/// @brief 全局单例状态（函数级 static，规避静态初始化顺序问题）。
ConfigState& config_state();

}  // namespace ca::config::detail

/// @brief visit 遍历到的条目快照。
struct ConfigEntry {
    /// 配置项名（即 JSON 顶层 key）。
    std::string name;
    /// 当前值的 JSON 文本表示（如 "42"、"true"、"[1,2]"）。
    std::string value;
    /// true 表示该 key 已加载但尚未被 lookup<T> 物化（仍存原始 JSON 值）。
    bool pending = false;
};

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
    /// @note 若 name 在此前的 load 中出现过（未物化），本次 lookup 用已加载值物化：
    ///       转换成功以加载值为初值；转换失败/类型不符退回本次 default。
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

        // 未物化命中：用已加载值物化；转换失败/类型不符退回本次 default
        T init_value = default_value;
        const auto pending_it = state.pending.find(name);
        if (pending_it != state.pending.end()) {
            const std::shared_ptr<ca::json::JsonDocument> document = pending_it->second;
            const ca::json::JsonValue* loaded =
                document->root().find(ca::str::Utf8StringRef::from_string_view(name));
            if (loaded != nullptr) {
                auto converted = JsonCast<T>::from_json(*loaded);
                if (converted.is_ok()) {
                    init_value = std::move(converted).unwrap();
                }
            }
            state.pending.erase(pending_it);
        }

        auto var = std::make_shared<ConfigVar<T>>(name, init_value, description);
        state.vars.emplace(name, var);
        return var;
    }

    /// @brief 按名字查找配置项（类型擦除视图）。
    /// @param name 配置项名。
    /// @return 已注册返回 ConfigVarBase；不存在返回 nullptr。
    static std::shared_ptr<ConfigVarBase> lookup_base(const std::string& name);

    /// @brief 从 JSON 文本加载配置。
    /// @param json_text 完整 JSON 文本。
    /// @return 成功返回 Ok。失败分两类：
    ///         - 整体失败（非法 JSON / 顶层非 object）：内部状态零变化，Err 的 code 为
    ///           PARSE_FAILED / ROOT_NOT_OBJECT，keys 为空；
    ///         - 部分失败（单 key 类型不匹配/超范围、或该 key 的监听器回调抛出异常）：
    ///           其余 key 照常生效，返回 Err（code 为 TYPE_MISMATCH / LISTENER_FAILED，
    ///           混合时为 TYPE_MISMATCH），keys 列出应用失败的 key，message 带详情。
    /// @note 部分失败仍返回 Err 是有意取舍：调用方需要显式感知"配置未完整生效"，
    ///       而哪些 key 生效了可以从 keys 的补集推知（见模块设计文档）。
    /// @note 监听器回调在本函数持有注册表锁之外触发，回调中可安全再进 Config；
    ///       回调抛出的异常不会穿透本函数（捕获后计入失败 key），但该 key 的值
    ///       此刻已应用、回调链中排在异常之后的监听器不会执行。
    static Result<void, ConfigErrorInfo> load(const std::string& json_text);

    /// @brief 从 JSON 文件加载配置（等价于读全文后调 load）。
    /// @param path 文件路径（UTF-8），自动剥离 BOM。
    /// @return 读文件失败返回 Err（code=READ_FILE_FAILED，内部状态零变化）；
    ///         其余语义同 load()。
    static Result<void, ConfigErrorInfo> load_file(const std::string& path);

    /// @brief 遍历全部配置项（已注册 var + 未物化条目）。
    /// @param callback 对每个条目回调一次；callback 为空时为本 no-op。
    /// @note 回调在注册表锁之外执行，顺序不保证；value 是回调时刻的快照。
    static void visit(const std::function<void(const ConfigEntry&)>& callback);

    /// @brief 清空注册表（含已注册配置项与未物化条目）。
    /// @note 仅供测试与程序初始化阶段重建注册表使用；已逃逸出去的
    ///       shared_ptr<ConfigVar<T>> 仍有效，但不再受注册中心管理。
    static void clear();
};

}  // namespace ca::config
