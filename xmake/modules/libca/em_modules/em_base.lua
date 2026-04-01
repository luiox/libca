-- em_base module handler

local inject = import("libca.em_inject")

local function _normalize_impl_opt(key, value)
    if value == nil then
        return "std"
    end

    if type(value) ~= "string" then
        print("[libca.em][warn] em_base.%s expects string(std/custom), got %s; fallback to std", key, type(value))
        return "std"
    end

    local normalized = value:lower()
    if normalized ~= "std" and normalized ~= "custom" then
        print("[libca.em][warn] em_base.%s invalid option '%s', expected std/custom; fallback to std", key, tostring(value))
        return "std"
    end

    return normalized
end

function get_handler()
    return {
        deps = {},
        handle = function (target, state, opts)
            local src_root = path.join(state.root, "libca.em", "src")
            local base_dir = path.join(src_root, "em_base")

            opts = opts or {}
            local memory_impl = _normalize_impl_opt("memory_util", opts.memory_util)
            local string_impl = _normalize_impl_opt("string_util", opts.string_util)

            inject.add_include(target, state, src_root)
            inject.add_file(target, state, path.join(base_dir, "debug.c"))
            inject.add_file(target, state, path.join(base_dir, "memory_util.c"))
            inject.add_file(target, state, path.join(base_dir, "string_util.c"))

            if memory_impl == "custom" then
                inject.add_define(target, state, "USE_CUSTOM_MEMORY_UTIL_IMPL=1")
            end

            if string_impl == "custom" then
                inject.add_define(target, state, "USE_CUSTOM_STRING_UTIL_IMPL=1")
            end
        end
    }
end
