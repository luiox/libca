-- libca.em_registry: module handler dispatcher and optional driver overrides

local _modules = {}
local _drivers = {}
local _module_paths = {}

do
    local root = os.scriptdir()
    for _, f in ipairs(os.files(path.join(root, "em_modules", "*.lua"))) do
        local name = path.basename(f)
        _module_paths[name] = "libca.em_modules." .. name
    end
end

local function _validate_module_handler(name, handler)
    if type(handler) ~= "table" then
        raise("libca.em: module '%s' handler must be table", tostring(name))
    end

    handler.deps = handler.deps or {}
    local handle = handler.handle or handler.inject
    if type(handle) ~= "function" then
        raise("libca.em: module '%s' handler must define handle()", tostring(name))
    end
end

function register_module(name, handler)
    if type(name) ~= "string" or name == "" then
        raise("libca.em: module name must be non-empty string")
    end

    _validate_module_handler(name, handler)
    _modules[name] = handler
end

function register_driver(name, handler)
    if type(name) ~= "string" or name == "" then
        raise("libca.em: driver name must be non-empty string")
    end

    if type(handler) ~= "table" then
        raise("libca.em: driver '%s' handler must be table", tostring(name))
    end

    local handle = handler.handle or handler.inject
    if type(handle) ~= "function" then
        raise("libca.em: driver '%s' handler must define handle()", tostring(name))
    end
    if handler.deps ~= nil and type(handler.deps) ~= "table" then
        raise("libca.em: driver '%s' dependencies must be list(table)", tostring(name))
    end
    for _, dep_name in ipairs(handler.deps or {}) do
        if type(dep_name) ~= "string" or dep_name == "" then
            raise("libca.em: driver '%s' dependency must be non-empty string", tostring(name))
        end
    end

    _drivers[name] = handler
end

function get_module(name)
    local handler = _modules[name]
    if handler then
        return handler
    end

    local module_path = _module_paths[name]
    if not module_path then
        return nil
    end

    local mod = import(module_path)
    if type(mod) ~= "table" or type(mod.get_handler) ~= "function" then
        raise("libca.em: module '%s' must export get_handler()", tostring(name))
    end

    handler = mod.get_handler()
    register_module(name, handler)
    return _modules[name]
end

function get_driver(name)
    return _drivers[name]
end

function list_modules()
    local names = {}
    local seen = {}
    for name, _ in pairs(_module_paths) do
        seen[name] = true
        table.insert(names, name)
    end
    for name, _ in pairs(_modules) do
        if not seen[name] then
            table.insert(names, name)
        end
    end
    table.sort(names)
    return names
end
