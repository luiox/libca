target("ca-em_platform")
    set_kind("static")
    add_files("*.c")

-- async的单元测试
target("test-async")
    set_kind("binary")
    add_files("async.c", "ringbuffer.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- 软件定时器的单元测试
target("test-soft_timer")
    set_kind("binary")
    add_files("soft_timer.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
