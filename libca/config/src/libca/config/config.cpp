#include "libca/config/config.hpp"

#include "libca/fs/file_util.hpp"
#include "libca/fs/fs_error.hpp"

#include "libca/json/json_reader.hpp"
#include "libca/json/json_writer.hpp"

#include <utility>
#include <vector>

namespace ca::config {

namespace detail {

ConfigState& config_state()
{
    static ConfigState state;
    return state;
}

}  // namespace ca::config::detail

// ==================== 查询 ====================

std::shared_ptr<ConfigVarBase> Config::lookup_base(const std::string& name)
{
    detail::ConfigState& state = detail::config_state();
    std::shared_lock<std::shared_mutex> lock(state.mutex);
    const auto it = state.vars.find(name);
    return it != state.vars.end() ? it->second : nullptr;
}

// ==================== 加载 ====================

Result<void, ConfigErrorInfo> Config::load(const std::string& json_text)
{
    // 解析阶段不加锁：整体失败时注册表零变化
    auto parsed = ca::json::JsonReader::read(ca::str::Utf8StringRef::from_string_view(json_text));
    if (parsed.is_err()) {
        const ca::json::ParseError error = std::move(parsed).unwrap_err();
        ConfigErrorInfo info;
        info.code = ConfigError::PARSE_FAILED;
        info.message = "JSON 解析失败: " + error.message.to_std_string();
        return Err(std::move(info));
    }
    // document 持有所有字符串的 arena：本函数全程保活，apply 阶段的 JsonValue 指针才有效
    auto document = std::make_shared<ca::json::JsonDocument>(std::move(parsed).unwrap());
    if (!document->root().is_object()) {
        ConfigErrorInfo info;
        info.code = ConfigError::ROOT_NOT_OBJECT;
        info.message = "配置顶层必须是 JSON object";
        return Err(std::move(info));
    }

    // 应用清单装配（独占锁）：已注册 key 记入待应用列表，未认领 key 存入未物化表
    detail::ConfigState& state = detail::config_state();
    std::vector<std::pair<std::shared_ptr<ConfigVarBase>, const ca::json::JsonValue*>> apply_list;
    {
        std::unique_lock<std::shared_mutex> lock(state.mutex);
        for (const auto& member : document->root().as_object()) {
            const std::string key = member.first.to_std_string();
            const auto it = state.vars.find(key);
            if (it != state.vars.end()) {
                apply_list.emplace_back(it->second, &member.second);
            } else {
                // 未物化：key 与整份文档一起入表，等后续 lookup<T> 认领
                state.pending[key] = document;
            }
        }
    }

    // 锁外应用：set_from_json 内部会触发监听器回调，回调可能再进 Config（lookup/visit），
    // 持注册表锁调用会自锁死。代价是并发 load 的应用顺序不保证（见设计文档）。
    std::vector<std::string> failed_keys;
    for (const auto& apply : apply_list) {
        const auto result = apply.first->set_from_json(*apply.second);
        if (result.is_err()) {
            failed_keys.push_back(apply.first->name());
        }
    }

    if (!failed_keys.empty()) {
        ConfigErrorInfo info;
        info.code = ConfigError::TYPE_MISMATCH;
        info.message = std::to_string(failed_keys.size()) + " 个配置项应用失败: ";
        for (ca::usize i = 0; i < failed_keys.size(); ++i) {
            if (i > 0) {
                info.message += ", ";
            }
            info.message += "\"" + failed_keys[i] + "\"";
        }
        info.keys = std::move(failed_keys);
        return Err(std::move(info));
    }
    return Ok();
}

Result<void, ConfigErrorInfo> Config::load_file(const std::string& path)
{
    // BOM 自动剥离：配置文件常由记事本等编辑器产出，带 BOM 应容错
    const auto read_result = ca::fs::FileUtil::read_all_text(path, true);
    if (read_result.is_err()) {
        const auto fs_error = read_result.unwrap_err();
        ConfigErrorInfo info;
        info.code = ConfigError::READ_FILE_FAILED;
        info.message = std::string("读取配置文件失败: ") + ca::fs::to_string(fs_error) + ": " + path;
        return Err(std::move(info));
    }
    return load(std::move(read_result).unwrap());
}

// ==================== 遍历 ====================

void Config::visit(const std::function<void(const ConfigEntry&)>& callback)
{
    if (!callback) {
        return;
    }
    detail::ConfigState& state = detail::config_state();

    // 锁内只取指针快照，锁外渲染 JSON 文本（渲染不回写注册表，也不触发监听器）
    std::vector<std::shared_ptr<ConfigVarBase>> vars;
    std::vector<std::pair<std::string, std::shared_ptr<ca::json::JsonDocument>>> pending;
    {
        std::shared_lock<std::shared_mutex> lock(state.mutex);
        vars.reserve(state.vars.size());
        for (const auto& entry : state.vars) {
            vars.push_back(entry.second);
        }
        pending.reserve(state.pending.size());
        for (const auto& entry : state.pending) {
            pending.emplace_back(entry.first, entry.second);
        }
    }

    for (const auto& var : vars) {
        ConfigEntry item;
        item.name = var->name();
        item.pending = false;
        ca::json::JsonDocument document;
        document.root() = var->to_json(document);
        item.value = ca::json::JsonWriter::write(document).to_std_string();
        callback(item);
    }

    for (const auto& entry : pending) {
        ConfigEntry item;
        item.name = entry.first;
        item.pending = true;
        // clone 是浅引用：字符串仍在 entry.second 持有的文档 arena 内，生命周期安全
        ca::json::JsonDocument view;
        const ca::json::JsonValue* loaded =
            entry.second->root().find(ca::str::Utf8StringRef::from_string_view(entry.first));
        view.root() = loaded != nullptr ? loaded->clone() : ca::json::JsonValue();
        item.value = ca::json::JsonWriter::write(view).to_std_string();
        callback(item);
    }
}

// ==================== 生命周期 ====================

void Config::clear()
{
    detail::ConfigState& state = detail::config_state();
    std::unique_lock<std::shared_mutex> lock(state.mutex);
    state.vars.clear();
    state.pending.clear();
}

}  // namespace ca::config
