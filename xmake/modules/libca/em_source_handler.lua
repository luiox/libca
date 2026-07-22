-- Shared handler for modules with a fixed source list.

local inject = import("libca.em_inject")

function make(spec)
    if type(spec) ~= "table" or type(spec.name) ~= "string" or spec.name == "" then
        raise("libca.em: source handler requires module name")
    end
    if type(spec.sources) ~= "table" or #spec.sources == 0 then
        raise("libca.em: module '%s' sources must be non-empty list(table)", tostring(spec.name))
    end

    return {
        deps = spec.deps or {},
        handle = function (target, state)
            local module_dir = path.join(state.src_root, spec.name)
            inject.add_include(target, state, state.src_root)
            for _, source in ipairs(spec.sources) do
                local source_file = path.join(module_dir, source)
                if not os.isfile(source_file) then
                    raise("libca.em: module '%s' source not found: %s", tostring(spec.name), source_file)
                end
                inject.add_file(target, state, source_file)
            end
        end
    }
end
