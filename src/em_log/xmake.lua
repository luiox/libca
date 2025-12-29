-- 嵌入式日志库
target("ca-em_log")
    set_kind("static")
    add_files("*.c")
    add_deps("ca-em_base")  

target("test-log_util")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("log.c")
    add_deps("ca-em_base")

