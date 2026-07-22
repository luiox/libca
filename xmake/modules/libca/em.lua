-- libca.em: source-package manager (import entry)

local core = import("libca.em_core")
local registry = import("libca.em_registry")

function setup(target, opts)
    core.setup(target, opts, registry)
end

function add_libs(target, modules, opts)
    core.add_libs(target, modules, opts, registry)
end

function get_state(target)
    return core.get_state(target)
end

function register_module(name, spec)
    registry.register_module(name, spec)
end

function register_driver(name, spec)
    registry.register_driver(name, spec)
end

function list_modules()
    return registry.list_modules()
end
