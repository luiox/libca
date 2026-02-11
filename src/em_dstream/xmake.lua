-- ringbuffer的单元测试
target("test-ringbuffer")
    set_kind("binary")
    add_files("ringbuffer.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")

target("test-fixed_size_buffer")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("fixed_size_buffer.c")
    add_deps("libca.em_base")
