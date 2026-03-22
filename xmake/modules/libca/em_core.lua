-- libca.em_core: state lifecycle and module dispatch

local inject = import("libca.em_inject")

local _states = {}

local function _target_key(target)
    if type(target.name) == "function" then
        return target:name()
    end
    return tostring(target)
end

local function _deep_copy(src)
    if type(src) ~= "table" then
        return src
    end

    local dst = {}
    for k, v in pairs(src) do
        if type(v) == "table" then
            dst[k] = _deep_copy(v)
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
    local src_root = path.join(root, "libca.em", "src")
    return {
        root = root,
        src_root = src_root,
        debug = false,
        modules = {},
        options = {},
        debug_trace = {
            current_module = nil,
            modules = {}
        },
        injected = {
            files = {},
            includedirs = {},
            defines = {}
        }
    }
end

local function _debug_dump_module(state, name)
    if not state.debug then
        return
    end

    local trace = state.debug_trace.modules[name] or {}
    local files = trace.files or {}
    local includedirs = trace.includedirs or {}

    print("[libca.em][debug] module: %s", tostring(name))

    if #files == 0 then
        print("  files: (none)")
    else
        print("  files:")
        for _, f in ipairs(files) do
            print("    - %s", path.relative(f, state.root))
        end
    end

    if #includedirs == 0 then
        print("  includedirs: (none)")
    else
        print("  includedirs:")
        for _, d in ipairs(includedirs) do
            print("    - %s", path.relative(d, state.root))
        end
    end
end

local function _ensure_state(target)
    return _states[_target_key(target)]
end

local function _apply_module(target, state, name, opts, registry)
    local module_handler = registry.get_module(name)
    if not module_handler then
        raise("libca.em.add_libs: unsupported module '%s'", tostring(name))
    end

    for _, dep_name in ipairs(module_handler.deps or {}) do
        if not (state.modules[dep_name] and state.modules[dep_name].enable == true) then
            raise(
                "libca.em.add_libs: missing dependency (module=%s, dependency=%s, target=%s)",
                tostring(name),
                tostring(dep_name),
                tostring(_target_key(target))
            )
        end
    end

    -- Repeated add_libs(module) keeps only the latest options for that module.
    state.options[name] = _deep_copy(opts or {})

    local handle = module_handler.handle or module_handler.inject
    if type(handle) ~= "function" then
        raise("libca.em: module '%s' handler must define handle(target, state, opts, registry)", tostring(name))
    end

    state.debug_trace.current_module = name
    handle(target, state, state.options[name], registry)
    state.debug_trace.current_module = nil

    state.modules[name] = {enable = true}
    _debug_dump_module(state, name)
end

function setup(target, opts)
    _validate_target(target, "setup")

    opts = opts or {}
    local root = inject.to_abs(os.projectdir(), opts.root or "")
    if not root or root == "" or not os.isdir(root) then
        raise("libca.em.setup: invalid root '%s'", tostring(opts.root))
    end

    local state = _new_state(root)
    state.debug = opts.debug == true

    _states[_target_key(target)] = state
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
