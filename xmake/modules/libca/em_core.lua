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
    if type(target) ~= "table" or type(target.add) ~= "function"
        or type(target.set) ~= "function" or type(target.script) ~= "function" then
        raise("libca.em.%s: target object is required", where)
    end
end

local function _new_state(root)
    local src_root = path.join(root, "libca.em", "src")
    return {
        root = root,
        src_root = src_root,
        debug = false,
        requested = {},
        modules = {},
        options = {},
        finalized = false,
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

local function _get_module_handler(name, registry)
    local module_handler = registry.get_module(name)
    if not module_handler then
        raise("libca.em.add_libs: unsupported module '%s'", tostring(name))
    end
    return module_handler
end

local function _validate_module_name(name)
    if type(name) ~= "string" or name == "" then
        raise("libca.em.add_libs: module name must be non-empty string")
    end
end

local function _validate_module_options(name, opts)
    if opts == nil or opts == true then
        return {}
    end
    if type(opts) ~= "table" then
        raise("libca.em.add_libs: module '%s' options must be table or true", tostring(name))
    end
    return opts
end

local function _queue_module(state, name, opts, registry)
    _validate_module_name(name)
    _get_module_handler(name, registry)

    state.requested[name] = true
    state.options[name] = _deep_copy(_validate_module_options(name, opts))
end

local function _queue_modules(state, modules, opts, registry)
    if type(modules) == "string" then
        _queue_module(state, modules, opts, registry)
        return
    end

    if type(modules) ~= "table" then
        raise("libca.em.add_libs: modules must be module name or table")
    end
    if opts ~= nil then
        raise("libca.em.add_libs: batch form keeps options inside the modules table")
    end

    local count = 0
    for key, value in pairs(modules) do
        if type(key) == "number" then
            _queue_module(state, value, {}, registry)
        else
            _queue_module(state, key, value, registry)
        end
        count = count + 1
    end
    if count == 0 then
        raise("libca.em.add_libs: modules table must not be empty")
    end
end

local function _resolve_dependencies(name, handler, state, registry)
    local deps = {}
    local seen = {}

    local function append(items)
        if type(items) ~= "table" then
            raise("libca.em: module '%s' dependencies must be list(table)", tostring(name))
        end
        for _, dep_name in ipairs(items) do
            _validate_module_name(dep_name)
            if not seen[dep_name] then
                seen[dep_name] = true
                table.insert(deps, dep_name)
            end
        end
    end

    append(handler.deps or {})
    if type(handler.resolve_deps) == "function" then
        append(handler.resolve_deps(state.options[name], state, registry) or {})
    end
    table.sort(deps)
    return deps
end

local function _apply_module(target, state, name, module_handler, registry)
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

local function _finalize_modules(target, state, registry)
    if state.finalized then
        return
    end

    local handlers = {}
    local dependencies = {}
    local names = {}
    for name, _ in pairs(state.requested) do
        local handler = _get_module_handler(name, registry)
        handlers[name] = handler
        dependencies[name] = _resolve_dependencies(name, handler, state, registry)
        table.insert(names, name)
    end
    table.sort(names)

    for _, name in ipairs(names) do
        for _, dep_name in ipairs(dependencies[name]) do
            if not state.requested[dep_name] then
                raise(
                    "libca.em.add_libs: missing dependency (module=%s, dependency=%s, target=%s)",
                    tostring(name),
                    tostring(dep_name),
                    tostring(_target_key(target))
                )
            end
        end
    end

    local visiting = {}
    local visited = {}
    local function visit(name)
        if visited[name] then
            return
        end
        if visiting[name] then
            raise("libca.em.add_libs: dependency cycle contains module '%s'", tostring(name))
        end

        visiting[name] = true
        for _, dep_name in ipairs(dependencies[name]) do
            visit(dep_name)
        end
        visiting[name] = nil

        _apply_module(target, state, name, handlers[name], registry)
        visited[name] = true
    end

    for _, name in ipairs(names) do
        visit(name)
    end
    state.finalized = true
end

function setup(target, opts, registry)
    _validate_target(target, "setup")

    if _ensure_state(target) then
        raise("libca.em.setup: target '%s' is already initialized", tostring(_target_key(target)))
    end

    if opts == nil then
        opts = {}
    elseif type(opts) ~= "table" then
        raise("libca.em.setup: options must be table")
    end

    local root = inject.to_abs(os.projectdir(), opts.root or "")
    if not root or root == "" or not os.isdir(root) then
        raise("libca.em.setup: invalid root '%s'", tostring(opts.root))
    end
    local src_root = path.join(root, "libca.em", "src")
    if not os.isdir(src_root) then
        raise("libca.em.setup: root does not contain libca.em/src: %s", tostring(root))
    end

    local state = _new_state(root)
    state.debug = opts.debug == true

    _states[_target_key(target)] = state

    local previous_after_load = target:script("load_after")
    target:set("load_after", function (loaded_target)
        _finalize_modules(loaded_target, state, registry)
        if previous_after_load then
            previous_after_load(loaded_target)
        end
    end)
end

function add_libs(target, modules, opts, registry)
    _validate_target(target, "add_libs")

    local state = _ensure_state(target)
    if not state then
        raise("libca.em.add_libs: call setup(target, {root = ...}) first")
    end
    if state.finalized then
        raise("libca.em.add_libs: target '%s' modules are already finalized", tostring(_target_key(target)))
    end

    _queue_modules(state, modules, opts, registry)
end

function get_state(target)
    return _ensure_state(target)
end
