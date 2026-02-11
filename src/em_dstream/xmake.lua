-- ringbuffer的单元测试
target("test-ring_buffer")
    set_kind("binary")
    add_files("ring_buffer.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")

target("test-fixed_buffer")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("fixed_buffer.c", "ds_fixed_buffer.c")
    add_deps("libca.em_base")

target("test-pingpong_buffer")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("pingpong_buffer.c")
    add_deps("libca.em_base")
