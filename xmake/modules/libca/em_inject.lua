-- libca.em_inject: idempotent target injection helpers

function to_abs(root, p)
    if not p then
        return nil
    end
    return path.is_absolute(p) and p or path.absolute(p, root)
end

local function _add_unique(target, bucket, key, adder, value)
    if bucket[key] then
        return
    end
    bucket[key] = true
    adder(target, value)
end

function add_file(target, state, f)
    _add_unique(target, state.injected.files, f, function (t, v)
        t:add("files", v)
    end, f)
end

function add_include(target, state, d)
    _add_unique(target, state.injected.includedirs, d, function (t, v)
        t:add("includedirs", v)
    end, d)
end

function add_define(target, state, d)
    _add_unique(target, state.injected.defines, d, function (t, v)
        t:add("defines", v)
    end, d)
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
