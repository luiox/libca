-- em_log module handler

local inject = import("libca.em_inject")

function get_handler()
    return {
        deps = {"em_base", "em_dstream", "em_format", "em_platform"},
        handle = function (target, state, opts)
            opts = opts or {}

            local src_root = path.join(state.root, "libca.em", "src")
            local log_dir = path.join(src_root, "em_log")
            local backend = opts.backend or "simple_logger"

            if type(backend) ~= "string" or backend == "" then
                raise("libca.em: em_log.backend must be string")
            end

            if backend ~= "simple_logger" then
                raise("libca.em: em_log unsupported backend '%s'", tostring(backend))
            end

            inject.add_include(target, state, src_root)
            inject.add_file(target, state, path.join(log_dir, "log.c"))
            inject.add_file(target, state, path.join(log_dir, "simple_logger.c"))
        end
    }
end
