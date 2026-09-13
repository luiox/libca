#pragma once

/// @file config_error.hpp
/// @brief config 模块错误码与带详情的错误值。

#include <string>
#include <vector>

namespace ca::config {

/// @brief config 模块通用错误码。
enum class ConfigError
{
    INVALID_ARGUMENT,  ///< 参数非法（如 name 为空）
    PARSE_FAILED,      ///< JSON 语法解析失败（整个 load 拒绝）
    ROOT_NOT_OBJECT,   ///< 配置顶层不是 JSON object（整个 load 拒绝）
    TYPE_MISMATCH,     ///< 值类型与目标类型不符
    OUT_OF_RANGE,      ///< 整数值超出目标类型范围
    READ_FILE_FAILED,  ///< load_file 读取文件失败
    LISTENER_FAILED,   ///< 值变更监听器回调抛出异常（该 key 值已应用，计入失败）
};

/// @brief 将 ConfigError 转为稳定的调试字符串。
/// @param error config 错误码。
/// @return 错误描述字符串。
inline const char* to_string(ConfigError error) noexcept
{
    switch (error) {
    case ConfigError::INVALID_ARGUMENT:
        return "invalid argument";
    case ConfigError::PARSE_FAILED:
        return "json parse failed";
    case ConfigError::ROOT_NOT_OBJECT:
        return "config root is not a json object";
    case ConfigError::TYPE_MISMATCH:
        return "value type mismatch";
    case ConfigError::OUT_OF_RANGE:
        return "integer value out of range";
    case ConfigError::READ_FILE_FAILED:
        return "read config file failed";
    case ConfigError::LISTENER_FAILED:
        return "config listener callback threw";
    }
    return "unknown config error";
}

/// @brief 带详情的错误值：code 供程序分支，message/keys 供诊断与测试断言。
/// @note 作为 Config::load / Config::load_file 的 Result 错误类型；
///       整体失败（解析失败/顶层非 object）时 keys 为空；部分失败（单 key 类型不匹配/
///       超范围被跳过、或该 key 的监听器回调抛出异常）时 keys 列出应用失败的 key，
///       其余 key 照常生效。
struct ConfigErrorInfo
{
    /// 错误码。
    ConfigError code = ConfigError::INVALID_ARGUMENT;
    /// 人读详情（UTF-8，含动态 key 名）。
    std::string message;
    /// load 阶段类型不匹配被跳过的 key 列表。
    std::vector<std::string> keys;
};

}  // namespace ca::config
