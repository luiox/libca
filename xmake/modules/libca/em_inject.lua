-- libca.em_inject: idempotent target injection helpers

function to_abs(root, p)
    if not p then
        return nil
    end
    return path.is_absolute(p) and p or path.absolute(p, root)
end

local function _add_unique(target, bucket, key, adder, value)
    if bucket[key] then
        return false
    end
    bucket[key] = true
    adder(target, value)
    return true
end

local function _record_debug(state, kind, value)
    local debug_trace = state and state.debug_trace
    if not (state and state.debug and debug_trace) then
        return
    end

    local current = debug_trace.current_module
    if not current then
        return
    end

    local module_trace = debug_trace.modules[current]
    if not module_trace then
        module_trace = {files = {}, includedirs = {}, defines = {}}
        debug_trace.modules[current] = module_trace
    end

    table.insert(module_trace[kind], value)
end

function add_file(target, state, f)
    local added = _add_unique(target, state.injected.files, f, function (t, v)
        t:add("files", v)
    end, f)
    if added then
        _record_debug(state, "files", f)
    end
end

function add_include(target, state, d)
    local normalized = d
    if state and state.src_root then
        normalized = to_abs(state.root, d)
        if normalized ~= state.src_root then
            return
        end
    end

    local added = _add_unique(target, state.injected.includedirs, normalized, function (t, v)
        t:add("includedirs", v)
    end, normalized)
    if added then
        _record_debug(state, "includedirs", normalized)
    end
end

function add_define(target, state, d)
    local added = _add_unique(target, state.injected.defines, d, function (t, v)
        t:add("defines", v)
    end, d)
    if added then
        _record_debug(state, "defines", d)
    end
end

function add_files(target, state, files)
    for _, f in ipairs(files or {}) do
        add_file(target, state, f)
    end
end

function add_includes(target, state, dirs)
    for _, d in ipairs(dirs or {}) do
        add_include(target, state, d)
    end
end

function add_defines(target, state, defs)
    for _, d in ipairs(defs or {}) do
        add_define(target, state, d)
    end
end
