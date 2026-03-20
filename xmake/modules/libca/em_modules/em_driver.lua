-- em_driver module handler (interpreter pattern)

local interpreter = import("libca.em_driver_interpreter")

local function _normalize_driver_map(opts)
    opts = opts or {}
    local drivers = {}

    for k, v in pairs(opts) do
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

function get_handler()
    return {
        deps = {"em_base"},
        handle = function (target, state, opts, reg)
            local drivers = _normalize_driver_map(opts)
            for driver_name, driver_opts in pairs(drivers) do
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
