local src_dir = "$(projectdir)/src/em_platform"

target("test-async")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "async.c"), "test_async.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_util")

target("test-soft_timer")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "time_util.c"), "test_time_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
