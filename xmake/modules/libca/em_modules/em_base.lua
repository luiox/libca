-- em_base module handler

local inject = import("libca.em_inject")

function get_handler()
    return {
        deps = {},
        handle = function (target, state)
            local src_root = path.join(state.root, "libca.em", "src")
            local base_dir = path.join(src_root, "em_base")

            inject.add_include(target, state, src_root)
            inject.add_file(target, state, path.join(base_dir, "debug.c"))
        end
    }
end
