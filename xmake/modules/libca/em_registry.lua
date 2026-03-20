-- libca.em_registry: module/driver registration center

local _modules = {}
local _drivers = {}
local _module_paths = {}
local _driver_paths = {}

do
    local root = os.scriptdir()
    for _, f in ipairs(os.files(path.join(root, "em_modules", "*.lua"))) do
        local name = path.basename(f)
        _module_paths[name] = "libca.em_modules." .. name
    end
    for _, f in ipairs(os.files(path.join(root, "em_drivers", "*.lua"))) do
        local name = path.basename(f)
        _driver_paths[name] = "libca.em_drivers." .. name
    end
end

local function _load_spec(module_path, kind, name)
    local mod = import(module_path)
    if type(mod) ~= "table" or type(mod.get_spec) ~= "function" then
        raise("libca.em: %s module '%s' must export get_spec()", kind, name)
    end

    local spec = mod.get_spec()
    if type(spec) ~= "table" or type(spec.inject) ~= "function" then
        raise("libca.em: %s '%s' spec.inject must be function", kind, name)
    end

    spec.deps = spec.deps or {}
    return spec
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
    local spec = _modules[name]
    if spec then
        return spec
    end

    local module_path = _module_paths[name]
    if not module_path then
        return nil
    end

    spec = _load_spec(module_path, "module", name)
    register_module(name, spec)
    return _modules[name]
end

function get_driver(name)
    local spec = _drivers[name]
    if spec then
        return spec
    end

    local driver_path = _driver_paths[name]
    if not driver_path then
        return nil
    end

    spec = _load_spec(driver_path, "driver", name)
    register_driver(name, spec)
    return _drivers[name]
end
