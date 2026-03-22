-- em_util module handler

local inject = import("libca.em_inject")

function get_handler()
    return {
        deps = {"em_base"},
        handle = function (target, state)
            local src_root = path.join(state.root, "libca.em", "src")
            local util_dir = path.join(src_root, "em_util")

            inject.add_include(target, state, src_root)
            for _, f in ipairs(os.files(path.join(util_dir, "**.c"))) do
                local name = path.filename(f)
                if name:sub(1, 5) ~= "test-" then
                    inject.add_file(target, state, f)
                end
            end
        end
    }
end
