#include "libca/config/config.hpp"

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

// ==================== 生命周期 ====================

void Config::clear()
{
    detail::ConfigState& state = detail::config_state();
    std::unique_lock<std::shared_mutex> lock(state.mutex);
    state.vars.clear();
}

}  // namespace ca::config
