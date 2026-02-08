-- 嵌入式实用工具库
target("libca.em_util")
    set_kind("object")
    set_group("em")
    add_files("**.c")
    add_deps("libca.em_base")
    -- link libm for math functions (tanf, sqrtf, etc.)
    add_syslinks("m")

target("libca.em_util_static")
    set_kind("static")
    set_group("em")
    add_files("**.c")
    add_deps("libca.em_base")

target("test-crc")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("crc.c")
    add_deps("libca.em_base")

-- ringbuffer的单元测试
target("test-ringbuffer")
    set_kind("binary")
    add_files("ringbuffer.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")

target("test-pid")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("pid.c")
    add_deps("libca.em_base")

target("test-ping_pong_buffer")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("ping_pong_buffer.c")
    add_deps("libca.em_base")

target("test-filter")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("filter.c")
    add_deps("libca.em_base")
    -- link libm for math functions (tanf, sqrtf, etc.)
    add_syslinks("m")

-- 嵌入式容器库
target("libca.em_collection")
    set_kind("static")
    set_group("em")
    add_files("**.c")
    remove_files("test-*.c")
    add_deps("libca.em_base")

target("test-doubly_linked_list")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("doubly_linked_list.c")
    add_deps("libca.em_base")

target("test-queue")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("queue.c")
    add_files("doubly_linked_list.c")
    add_deps("libca.em_base")

target("test-stack")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("stack.c")
    add_deps("libca.em_base")

target("test-fixed_size_buffer")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("fixed_size_buffer.c")
    add_deps("libca.em_base")

-- mem_view 单元测试
target("test-mem_view")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("mem_view.c")
    add_deps("libca.em_base")

