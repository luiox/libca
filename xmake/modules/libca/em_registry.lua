-- libca.em_registry: module/driver registration center

local inject = import("libca.em_inject")

local _modules = {}
local _drivers = {}

local function _normalize_port_list(port)
    if port == nil then
        return {}
    end
    if type(port) == "string" then
        return {port}
    end
    if type(port) ~= "table" then
        raise("libca.em: em_driver.led.port must be list(table) or string")
    end
    return port
end

function register_module(name, spec)
    if type(name) ~= "string" or name == "" then
        raise("libca.em: module name must be non-empty string")
    end
    if type(spec) ~= "table" or type(spec.inject) ~= "function" then
        raise("libca.em: module '%s' spec.inject must be function", tostring(name))
    end
    spec.deps = spec.deps or {}
    _modules[name] = spec
end

function register_driver(name, spec)
    if type(name) ~= "string" or name == "" then
        raise("libca.em: driver name must be non-empty string")
    end
    if type(spec) ~= "table" or type(spec.inject) ~= "function" then
        raise("libca.em: driver '%s' spec.inject must be function", tostring(name))
    end
    _drivers[name] = spec
end

function get_module(name)
    return _modules[name]
end

function get_driver(name)
    return _drivers[name]
end

register_module("em_base", {
    inject = function (target, state)
        local src_root = path.join(state.root, "src")
        local base_dir = path.join(src_root, "em_base")

        inject.add_include(target, state, src_root)
        inject.add_file(target, state, path.join(base_dir, "datatype.c"))
        inject.add_file(target, state, path.join(base_dir, "debug.c"))
        inject.add_file(target, state, path.join(base_dir, "compiler_compat.c"))
    end
})

register_driver("led", {
    inject = function (target, state, opts)
        opts = opts or {}

        local src_root = path.join(state.root, "src")
        local led_dir = path.join(src_root, "em_driver", "led")
        local mode = opts.mode or "extern"

        if mode ~= "extern" and mode ~= "dynamic" then
            raise("libca.em: em_driver.led.mode must be 'extern' or 'dynamic'")
        end

        inject.add_include(target, state, src_root)
        inject.add_file(target, state, path.join(led_dir, "led.c"))

        if mode == "dynamic" then
            inject.add_define(target, state, "LIBCA_LED_PORT_MODE=2")
            return
        end

        inject.add_define(target, state, "LIBCA_LED_PORT_MODE=1")

        local ports = _normalize_port_list(opts.port)
        if #ports > 0 then
            for _, p in ipairs(ports) do
                if type(p) ~= "string" then
                    raise("libca.em: em_driver.led.port item must be string path")
                end
                inject.add_file(target, state, inject.to_abs(os.projectdir(), p))
            end
            return
        end

        inject.add_file(target, state, path.join(led_dir, "port_led.c"))
    end
})

register_module("em_driver", {
    deps = {"em_base"},
    inject = function (target, state, opts, reg)
        opts = opts or {}

        for driver_name, driver_opts in pairs(opts) do
            local spec = reg.get_driver(driver_name)
            if not spec then
                raise("libca.em: unsupported em_driver '%s'", tostring(driver_name))
            end
            spec.inject(target, state, driver_opts)
        end
    end
})