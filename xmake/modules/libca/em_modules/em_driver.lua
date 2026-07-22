-- em_driver module handler (interpreter pattern)

local interpreter = import("libca.em_driver_interpreter")

local function _normalize_driver_map(opts)
    opts = opts or {}
    local drivers = {}

    for k, v in pairs(opts) do
        if type(k) ~= "string" or k == "" then
            raise("libca.em: em_driver name must be non-empty string")
        end
        if type(v) == "table" then
            drivers[k] = v
        elseif v == true then
            drivers[k] = {}
        elseif v ~= false then
            raise("libca.em: em_driver.%s config must be table/bool", tostring(k))
        end
    end

    return drivers
end

local function _driver_names(drivers)
    local names = {}
    for name, _ in pairs(drivers) do
        table.insert(names, name)
    end
    table.sort(names)
    return names
end

function get_handler()
    return {
        deps = {"em_base"},
        resolve_deps = function (opts, state, reg)
            local deps = {}
            local seen = {}
            local drivers = _normalize_driver_map(opts)
            for _, driver_name in ipairs(_driver_names(drivers)) do
                local override = reg.get_driver(driver_name)
                local driver_deps
                if override then
                    driver_deps = override.deps or {}
                else
                    driver_deps = interpreter.get_dependencies(state, driver_name)
                end
                for _, dep_name in ipairs(driver_deps or {}) do
                    if not seen[dep_name] then
                        seen[dep_name] = true
                        table.insert(deps, dep_name)
                    end
                end
            end
            return deps
        end,
        handle = function (target, state, opts, reg)
            local drivers = _normalize_driver_map(opts)
            for _, driver_name in ipairs(_driver_names(drivers)) do
                local driver_opts = drivers[driver_name]
                local override = reg.get_driver(driver_name)
                if override then
                    local fn = override.handle or override.inject
                    fn(target, state, driver_opts, reg)
                else
                    interpreter.handle_driver(target, state, driver_name, driver_opts)
                end
            end
        end
    }
end
