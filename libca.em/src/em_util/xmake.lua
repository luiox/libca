-- 嵌入式实用工具库
target("libca.em_util")
    set_kind("object")
    set_group("em")
    add_files("**.c")
    add_deps("libca.em_base")
    -- link libm for math functions (tanf, sqrtf, etc.)
    -- add_syslinks("m")

target("libca.em_util_static")
    set_kind("static")
    set_group("em")
    add_files("**.c")
    add_deps("libca.em_base")

-- 嵌入式容器库
target("libca.em_collection")
    set_kind("static")
    set_group("em")
    add_files("**.c")
    remove_files("test-*.c")
    add_deps("libca.em_base")

