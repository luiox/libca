local inject = import("libca.em_inject")

function get_handler()
    return {
        deps = {"em_base"},
        handle = function (target, state, opts)
            opts = opts or {}
            local test_dir = path.join(state.src_root, "em_test")

            inject.add_include(target, state, state.src_root)
            inject.add_file(target, state, path.join(test_dir, "test.c"))
            if opts.default_main == true then
                inject.add_define(target, state, "TEST_SELF_MAIN=1")
                inject.add_file(target, state, path.join(test_dir, "test_main.c"))
            end
            if opts.file_recorder == true then
                inject.add_file(target, state, path.join(test_dir, "simple_file_recorder.c"))
            end
            if opts.test_enable == true then
                inject.add_define(target, state, "TEST_ENABLE=1")
            end
        end
    }
end
