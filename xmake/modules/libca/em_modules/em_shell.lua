-- em_shell module handler

local inject = import("libca.em_inject")

function get_handler()
    return {
        deps = {"em_base"},
        handle = function (target, state)
            local src_root = path.join(state.root, "src")
            inject.add_include(target, state, src_root)
            inject.add_file(target, state, path.join(src_root, "em_shell", "shell.c"))
        end
    }
end
