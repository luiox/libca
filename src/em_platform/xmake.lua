target("libca.em_platform")
    set_kind("static")
    add_files("*.c")

local em_platform_test_dir = "$(projectdir)/tests/em_platform"

-- async的单元测试
target("test-async")
    set_kind("binary")
    add_includedirs(".", em_platform_test_dir)
    add_files("async.c", path.join(em_platform_test_dir, "test_async.c"))
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_util")

-- 软件定时器的单元测试
target("test-soft_timer")
    set_kind("binary")
    add_includedirs(".", em_platform_test_dir)
    add_files("time_util.c", path.join(em_platform_test_dir, "test_time_util.c"))
    add_rules("em_test", { test_enable = true, use_default_main = true })
