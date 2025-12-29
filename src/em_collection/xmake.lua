-- 嵌入式容器库
target("ca-em_collection")
    set_kind("static")
    set_group("em")
    add_files("**.c")
    remove_files("test-*.c")
    add_deps("ca-em_base")

target("test-doubly_linked_list")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("doubly_linked_list.c")
    add_deps("ca-em_base")

target("test-queue")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("queue.c")
    add_files("doubly_linked_list.c")
    add_deps("ca-em_base")

target("test-stack")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("stack.c")
    add_deps("ca-em_base")

