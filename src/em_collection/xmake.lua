-- 嵌入式容器库
target("ca-em_collection")
    set_kind("static")
    add_files("**.c")
    remove_files("test-*.c")
    add_deps("ca-em_base")

