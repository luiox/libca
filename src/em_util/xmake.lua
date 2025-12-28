-- 嵌入式实用工具库
target("ca-em_util")
    set_kind("static")
    add_files("**.c")
    add_deps("ca-em_base")


