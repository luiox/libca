-- em_log module handler

local inject = import("libca.em_inject")

function get_handler()
    return {
        deps = {"em_base"},
        handle = function (target, state, opts)
            opts = opts or {}

            local src_root = path.join(state.root, "src")
            local log_dir = path.join(src_root, "em_log")
            local backend = opts.backend or "simple_logger"

            if type(backend) ~= "string" or backend == "" then
                raise("libca.em: em_log.backend must be string")
            end

            local backend_file = path.join(log_dir, backend .. ".c")
            if not os.isfile(backend_file) then
                raise("libca.em: em_log backend source not found: %s", backend_file)
            end

            inject.add_include(target, state, src_root)
            inject.add_file(target, state, path.join(log_dir, "log.c"))
            inject.add_file(target, state, backend_file)
        end
    }
end
