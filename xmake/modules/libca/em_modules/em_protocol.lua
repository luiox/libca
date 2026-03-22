-- em_protocol module handler

local inject = import("libca.em_inject")

function get_handler()
    return {
        deps = {"em_base", "em_util"},
        handle = function (target, state)
            local src_root = path.join(state.root, "libca.em", "src")
            local protocol_dir = path.join(src_root, "em_protocol")

            inject.add_include(target, state, src_root)
            for _, f in ipairs(os.files(path.join(protocol_dir, "**.c"))) do
                local name = path.filename(f)
                if name:sub(1, 5) ~= "test-" then
                    inject.add_file(target, state, f)
                end
            end
        end
    }
end
