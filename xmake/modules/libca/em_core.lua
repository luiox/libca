-- libca.em_core: state lifecycle and module dispatch

local inject = import("libca.em_inject")

local _states = {}

local function _target_key(target)
    if type(target.name) == "function" then
        return target:name()
    end
    return tostring(target)
end

local function _deep_merge(dst, src)
    if type(src) ~= "table" then
        return dst
    end
    for k, v in pairs(src) do
        if type(v) == "table" and type(dst[k]) == "table" then
            _deep_merge(dst[k], v)
        else
            dst[k] = v
        end
    end
    return dst
end

local function _validate_target(target, where)
    if type(target) ~= "table" or type(target.add) ~= "function" then
        raise("libca.em.%s: target object is required", where)
    end
end

local function _new_state(root)
    return {
        root = root,
        modules = {},
        options = {},
        injected = {
            files = {},
            includedirs = {},
            defines = {}
        }
    }
end

local function _ensure_state(target)
    return _states[_target_key(target)]
end

local function _apply_module(target, state, name, opts, registry, visited)
    visited = visited or {}
    if visited[name] then
        return
    end
    visited[name] = true

    local module_handler = registry.get_module(name)
    if not module_handler then
        raise("libca.em.add_libs: unsupported module '%s'", tostring(name))
    end

    for _, dep_name in ipairs(module_handler.deps or {}) do
        _apply_module(target, state, dep_name, {}, registry, visited)
    end

    state.options[name] = state.options[name] or {}
    _deep_merge(state.options[name], opts or {})

    local handle = module_handler.handle or module_handler.inject
    if type(handle) ~= "function" then
        raise("libca.em: module '%s' handler must define handle(target, state, opts, registry)", tostring(name))
    end

    handle(target, state, state.options[name], registry)
    state.modules[name] = {enable = true}
end

function setup(target, opts)
    _validate_target(target, "setup")

    opts = opts or {}
    local root = inject.to_abs(os.projectdir(), opts.root or "")
    if not root or root == "" or not os.isdir(root) then
        raise("libca.em.setup: invalid root '%s'", tostring(opts.root))
    end

    _states[_target_key(target)] = _new_state(root)
end

function add_libs(target, name, opts, registry)
    _validate_target(target, "add_libs")

    local state = _ensure_state(target)
    if not state then
        raise("libca.em.add_libs: call setup(target, {root = ...}) first")
    end

    _apply_module(target, state, name, opts or {}, registry)
end

function get_state(target)
    return _ensure_state(target)
end
