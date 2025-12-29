target("ca-em_base")
    set_kind("static")
    add_files("**.c")
    add_includedirs(".", { public = true })

-- ringbuffer的单元测试
target("test-ringbuffer")
    set_kind("binary")
    add_files("ringbuffer.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- 软件定时器的单元测试
target("test-soft_timer")
    set_kind("binary")
    add_files("soft_timer.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- string_util的单元测试
target("test-string_util")
    set_kind("binary")
    add_files("string_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- datatype的单元测试
target("test-datatype")
    set_kind("binary")
    add_files("datatype.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
