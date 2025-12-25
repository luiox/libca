-- 嵌入式日志库
target("ca-em_log")
    set_kind("static")
    add_files("**.c")
    add_deps("ca-em_base")  

