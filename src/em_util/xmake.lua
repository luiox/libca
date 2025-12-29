-- 嵌入式实用工具库
target("ca-em_util")
    set_kind("static")
    add_files("**.c")
    add_deps("ca-em_base")

target("test-ringbuffer_util")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("ringbuffer_util.c")
    add_deps("ca-em_base")

target("test-crc")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("crc.c")
    add_deps("ca-em_base")

target("test-pid")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("pid.c")
    add_deps("ca-em_base")

target("test-ping_pong_buffer")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("ping_pong_buffer.c")
    add_deps("ca-em_base")


