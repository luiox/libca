-- 嵌入式驱动库
target("ca-em_driver")
    set_kind("static")
    add_files("**.c")
    add_deps("ca-em_base")
